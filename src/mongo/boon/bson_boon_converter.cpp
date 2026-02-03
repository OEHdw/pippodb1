/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/bson_boon_converter.h"
#include "mongo/bson/bsonobjbuilder.h"

#include <cstring>

namespace mongo {
namespace boon {

// ========== Schema Extraction ==========

Schema extractSchema(const BSONObj& obj) {
    Schema schema;

    for (const auto& elem : obj) {
        schema.addField(elem.fieldName(), bsonTypeToBoonType(elem.type()));
    }

    return schema;
}

// ========== Uniformity Detection ==========

bool isUniformArray(const BSONObj& array, Schema* outSchema) {
    if (array.isEmpty()) {
        return false;
    }

    Schema firstSchema;
    bool first = true;

    for (const auto& elem : array) {
        // Array elements must be objects for uniform array
        if (elem.type() != BSONType::Object) {
            return false;
        }

        Schema elemSchema = extractSchema(elem.Obj());

        if (first) {
            firstSchema = elemSchema;
            first = false;
        } else {
            // Compare schemas
            if (!(elemSchema == firstSchema)) {
                return false;
            }
        }
    }

    if (outSchema) {
        *outSchema = firstSchema;
    }

    return true;
}

bool isCompactArray(const BSONObj& array, BOONType* elementType) {
    if (array.isEmpty()) {
        return false;
    }

    BSONType firstType = BSONType::EOO;

    for (const auto& elem : array) {
        BSONType t = elem.type();

        // Only primitives can be in compact arrays
        if (t == BSONType::Object || t == BSONType::Array) {
            return false;
        }

        if (firstType == BSONType::EOO) {
            firstType = t;
        } else if (t != firstType) {
            // Allow numeric type mixing (all numbers can be compact)
            bool firstIsNumeric = (firstType == BSONType::NumberDouble ||
                                   firstType == BSONType::NumberInt ||
                                   firstType == BSONType::NumberLong ||
                                   firstType == BSONType::NumberDecimal);
            bool currIsNumeric = (t == BSONType::NumberDouble ||
                                  t == BSONType::NumberInt ||
                                  t == BSONType::NumberLong ||
                                  t == BSONType::NumberDecimal);

            if (!(firstIsNumeric && currIsNumeric)) {
                return false;
            }
        }
    }

    if (elementType) {
        *elementType = bsonTypeToBoonType(firstType);
    }

    return true;
}

// ========== BSON Array to UniformArray ==========

std::vector<char> bsonArrayToUniformArray(const BSONObj& array,
                                          const Schema& schema) {
    UniformArrayBuilder builder;
    builder.setSchema(schema);

    for (const auto& arrayElem : array) {
        if (arrayElem.type() != BSONType::Object) {
            continue;  // Skip non-objects (shouldn't happen if isUniformArray passed)
        }

        builder.startRow();

        BSONObj obj = arrayElem.Obj();

        // Iterate schema fields to ensure correct order
        for (size_t i = 0; i < schema.numFields(); ++i) {
            const auto& field = schema.field(i);
            BSONElement elem = obj[field.name];

            switch (field.type) {
                case BOONType::NumberDouble:
                    builder.appendDouble(elem.numberDouble());
                    break;
                case BOONType::NumberInt:
                    builder.appendInt32(elem.numberInt());
                    break;
                case BOONType::NumberLong:
                    builder.appendInt64(elem.numberLong());
                    break;
                case BOONType::String:
                    builder.appendString(elem.valueStringData());
                    break;
                case BOONType::Bool:
                    builder.appendBool(elem.boolean());
                    break;
                // TODO: Add more types as needed
                default:
                    // For unsupported types, this conversion shouldn't have been attempted
                    break;
            }
        }

        builder.finishRow();
    }

    return builder.done();
}

// ========== UniformArray to BSON Array ==========

BSONObj uniformArrayToBsonArray(const UniformArray& arr) {
    BSONObjBuilder arrayBuilder;

    int index = 0;
    for (const auto& row : arr) {
        BSONObjBuilder objBuilder(arrayBuilder.subobjStart(std::to_string(index)));

        const Schema& schema = arr.schema();
        for (size_t i = 0; i < schema.numFields(); ++i) {
            const auto& field = schema.field(i);

            switch (field.type) {
                case BOONType::NumberDouble:
                    objBuilder.append(field.name, row.getDouble(i));
                    break;
                case BOONType::NumberInt:
                    objBuilder.append(field.name, row.getInt32(i));
                    break;
                case BOONType::NumberLong:
                    objBuilder.append(field.name, row.getInt64(i));
                    break;
                case BOONType::String:
                    objBuilder.append(field.name, row.getString(i));
                    break;
                case BOONType::Bool:
                    objBuilder.append(field.name, row.getBool(i));
                    break;
                default:
                    // TODO: Handle more types
                    break;
            }
        }

        objBuilder.done();
        ++index;
    }

    return arrayBuilder.obj();
}

// ========== Main Conversion Functions ==========

namespace {

// Helper to convert recursively
void convertBsonToBoonRecursive(const BSONObj& bson,
                                 BOONObjBuilder& builder,
                                 const ConversionOptions& options,
                                 ConversionStats* stats,
                                 int depth) {
    if (depth > options.maxDepth) {
        // Too deep, just copy as-is
        // TODO: Implement fallback
        return;
    }

    for (const auto& elem : bson) {
        StringData fieldName = elem.fieldNameStringData();

        switch (elem.type()) {
            case BSONType::Object: {
                // Recursively convert nested object
                BOONObjBuilder subBuilder;
                convertBsonToBoonRecursive(elem.Obj(), subBuilder, options, stats, depth + 1);
                builder.append(fieldName, subBuilder.obj());
                break;
            }

            case BSONType::Array: {
                BSONObj arr = elem.Obj();

                // Check if this is a uniform array
                Schema schema;
                if (options.enableUniformArrays &&
                    arr.nFields() >= static_cast<int>(options.minUniformArraySize) &&
                    isUniformArray(arr, &schema)) {

                    // Convert to UniformArray
                    auto uniformData = bsonArrayToUniformArray(arr, schema);
                    UniformArray uniformArr(uniformData.data());

                    builder.appendUniformArray(fieldName, uniformArr);

                    if (stats) {
                        stats->uniformArraysCreated++;
                        // Estimate savings: original - converted
                        int64_t originalArraySize = arr.objsize();
                        int64_t convertedArraySize = uniformData.size();
                        stats->bytesSaved += (originalArraySize - convertedArraySize);
                    }
                }
                // TODO: Check for compact array
                else {
                    // Keep as regular array - recursively convert elements
                    BOONObjBuilder arrBuilder;
                    convertBsonToBoonRecursive(arr, arrBuilder, options, stats, depth + 1);
                    // TODO: Proper array handling
                }
                break;
            }

            // Pass through simple types
            case BSONType::NumberDouble:
                builder.append(fieldName, elem.numberDouble());
                break;
            case BSONType::String:
                builder.append(fieldName, elem.valueStringData());
                break;
            case BSONType::NumberInt:
                builder.append(fieldName, elem.numberInt());
                break;
            case BSONType::NumberLong:
                builder.append(fieldName, elem.numberLong());
                break;
            case BSONType::Bool:
                builder.append(fieldName, elem.boolean());
                break;
            case BSONType::jstNULL:
                builder.appendNull(fieldName);
                break;
            case BSONType::jstOID:
                builder.append(fieldName, elem.OID());
                break;

            // TODO: Handle remaining types
            default:
                // For now, skip unsupported types
                break;
        }
    }
}

void convertBoonToBsonRecursive(const BOONObj& boon,
                                 BSONObjBuilder& builder,
                                 int depth) {
    if (depth > 100) {
        return;  // Prevent stack overflow
    }

    for (auto it = boon.begin(); it.more(); ) {
        BOONElement elem = it.next();
        StringData fieldName = elem.fieldNameStringData();

        switch (elem.boonType()) {
            case BOONType::Object: {
                BSONObjBuilder subBuilder(builder.subobjStart(fieldName));
                convertBoonToBsonRecursive(elem.Obj(), subBuilder, depth + 1);
                subBuilder.done();
                break;
            }

            case BOONType::Array: {
                BSONObjBuilder arrBuilder(builder.subarrayStart(fieldName));
                convertBoonToBsonRecursive(elem.Array(), arrBuilder, depth + 1);
                arrBuilder.done();
                break;
            }

            case BOONType::UniformArray: {
                // Expand UniformArray to regular BSON array
                BSONObj expanded = uniformArrayToBsonArray(elem.uniformArray());
                builder.appendArray(fieldName, expanded);
                break;
            }

            case BOONType::CompactArray: {
                // TODO: Expand CompactArray
                break;
            }

            // Simple types
            case BOONType::NumberDouble:
                builder.append(fieldName, elem.Double());
                break;
            case BOONType::String:
                builder.append(fieldName, elem.String());
                break;
            case BOONType::NumberInt:
                builder.append(fieldName, elem.Int());
                break;
            case BOONType::NumberLong:
                builder.append(fieldName, elem.Long());
                break;
            case BOONType::Bool:
                builder.append(fieldName, elem.Bool());
                break;
            case BOONType::jstNULL:
                builder.appendNull(fieldName);
                break;
            case BOONType::OID:
                builder.append(fieldName, elem.OID());
                break;
            case BOONType::Date:
                builder.append(fieldName, elem.Date());
                break;

            default:
                // Skip unsupported types
                break;
        }
    }
}

}  // namespace

BOONObj bsonToBoon(const BSONObj& bson,
                   const ConversionOptions& options,
                   ConversionStats* stats) {
    if (stats) {
        stats->documentsConverted = 0;
        stats->uniformArraysCreated = 0;
        stats->compactArraysCreated = 0;
        stats->bytesSaved = 0;
        stats->originalSize = bson.objsize();
    }

    BOONObjBuilder builder;
    convertBsonToBoonRecursive(bson, builder, options, stats, 0);

    BOONObj result = builder.obj();

    if (stats) {
        stats->documentsConverted = 1;
        stats->convertedSize = result.objsize();
    }

    return result;
}

BSONObj boonToBson(const BOONObj& boon) {
    BSONObjBuilder builder;
    convertBoonToBsonRecursive(boon, builder, 0);
    return builder.obj();
}

}  // namespace boon
}  // namespace mongo
