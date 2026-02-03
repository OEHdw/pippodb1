/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    Server Side Public License for more details.
 */

#pragma once

#include <cstdint>
#include <string>

namespace mongo {
namespace boon {

/**
 * BOON Type Codes
 *
 * BOON (Binary Object-Oriented Notation) extends BSON with optimized types
 * for uniform arrays and compact storage.
 *
 * Types 0x00-0x13 are BSON-compatible.
 * Types 0x14-0x1F are BOON extensions.
 */
enum class BOONType : uint8_t {
    // ========== BSON-COMPATIBLE TYPES ==========

    /** End of object marker */
    EOO = 0x00,

    /** 64-bit IEEE 754 floating point */
    NumberDouble = 0x01,

    /** UTF-8 string */
    String = 0x02,

    /** Embedded document */
    Object = 0x03,

    /** Array (traditional, with "0", "1", "2" keys) */
    Array = 0x04,

    /** Binary data */
    BinData = 0x05,

    /** Undefined (deprecated) */
    Undefined = 0x06,

    /** ObjectId (12 bytes) */
    OID = 0x07,

    /** Boolean */
    Bool = 0x08,

    /** UTC datetime (milliseconds since epoch) */
    Date = 0x09,

    /** Null value */
    jstNULL = 0x0A,

    /** Regular expression */
    RegEx = 0x0B,

    /** DBRef (deprecated) */
    DBRef = 0x0C,

    /** JavaScript code */
    Code = 0x0D,

    /** Symbol (deprecated) */
    Symbol = 0x0E,

    /** JavaScript code with scope */
    CodeWScope = 0x0F,

    /** 32-bit signed integer */
    NumberInt = 0x10,

    /** Timestamp (internal) */
    bsonTimestamp = 0x11,

    /** 64-bit signed integer */
    NumberLong = 0x12,

    /** 128-bit decimal floating point */
    NumberDecimal = 0x13,

    // ========== BOON EXTENSION TYPES ==========

    /**
     * Uniform Array - THE KEY INNOVATION
     *
     * Instead of storing:
     *   [{name:"A", age:1}, {name:"B", age:2}, {name:"C", age:3}]
     * As:
     *   "0" -> {name:"A", age:1}
     *   "1" -> {name:"B", age:2}
     *   "2" -> {name:"C", age:3}
     *
     * BOON stores:
     *   Schema: [("name", String), ("age", Int32)]
     *   Rows: [["A", 1], ["B", 2], ["C", 3]]
     *
     * ~40% smaller for typical data!
     */
    UniformArray = 0x14,

    /**
     * Compact Array - array of primitives without index keys
     *
     * Instead of: "0"->1, "1"->2, "2"->3
     * Stores: [1, 2, 3] directly with element type header
     */
    CompactArray = 0x15,

    /**
     * Schema Reference - reference to externally defined schema
     * For when multiple documents share the same schema.
     */
    SchemaRef = 0x16,

    // ========== SPECIAL TYPES ==========

    /** Maximum key (for sorting) */
    MaxKey = 0x7F,

    /** Minimum key (for sorting) */
    MinKey = 0xFF,
};

/**
 * Convert BOONType to human-readable string
 */
inline const char* typeName(BOONType type) {
    switch (type) {
        case BOONType::EOO: return "EOO";
        case BOONType::NumberDouble: return "double";
        case BOONType::String: return "string";
        case BOONType::Object: return "object";
        case BOONType::Array: return "array";
        case BOONType::BinData: return "binData";
        case BOONType::Undefined: return "undefined";
        case BOONType::OID: return "objectId";
        case BOONType::Bool: return "bool";
        case BOONType::Date: return "date";
        case BOONType::jstNULL: return "null";
        case BOONType::RegEx: return "regex";
        case BOONType::DBRef: return "dbRef";
        case BOONType::Code: return "javascript";
        case BOONType::Symbol: return "symbol";
        case BOONType::CodeWScope: return "javascriptWithScope";
        case BOONType::NumberInt: return "int";
        case BOONType::bsonTimestamp: return "timestamp";
        case BOONType::NumberLong: return "long";
        case BOONType::NumberDecimal: return "decimal";
        case BOONType::UniformArray: return "uniformArray";
        case BOONType::CompactArray: return "compactArray";
        case BOONType::SchemaRef: return "schemaRef";
        case BOONType::MaxKey: return "maxKey";
        case BOONType::MinKey: return "minKey";
        default: return "invalid";
    }
}

/**
 * Check if type is a BOON extension (not standard BSON)
 */
inline bool isBOONExtension(BOONType type) {
    uint8_t t = static_cast<uint8_t>(type);
    return t >= 0x14 && t <= 0x1F;
}

/**
 * Check if type is numeric
 */
inline bool isNumericType(BOONType type) {
    return type == BOONType::NumberDouble ||
           type == BOONType::NumberInt ||
           type == BOONType::NumberLong ||
           type == BOONType::NumberDecimal;
}

/**
 * Check if type has fixed size
 */
inline bool isFixedSizeType(BOONType type) {
    switch (type) {
        case BOONType::NumberDouble:
        case BOONType::OID:
        case BOONType::Bool:
        case BOONType::Date:
        case BOONType::jstNULL:
        case BOONType::NumberInt:
        case BOONType::bsonTimestamp:
        case BOONType::NumberLong:
        case BOONType::NumberDecimal:
        case BOONType::MaxKey:
        case BOONType::MinKey:
        case BOONType::EOO:
            return true;
        default:
            return false;
    }
}

/**
 * Get fixed size for type (returns 0 for variable-size types)
 */
inline size_t fixedSize(BOONType type) {
    switch (type) {
        case BOONType::NumberDouble: return 8;
        case BOONType::OID: return 12;
        case BOONType::Bool: return 1;
        case BOONType::Date: return 8;
        case BOONType::jstNULL: return 0;
        case BOONType::NumberInt: return 4;
        case BOONType::bsonTimestamp: return 8;
        case BOONType::NumberLong: return 8;
        case BOONType::NumberDecimal: return 16;
        case BOONType::MaxKey: return 0;
        case BOONType::MinKey: return 0;
        case BOONType::EOO: return 0;
        default: return 0;  // Variable size
    }
}

/**
 * Canonical type ordering for index comparisons.
 *
 * Types with the same canonical value compare as equal for ordering purposes.
 * This is critical for index correctness!
 */
inline int8_t canonicalType(BOONType type) {
    switch (type) {
        case BOONType::MinKey: return -1;
        case BOONType::EOO: return 0;
        case BOONType::jstNULL: return 5;
        case BOONType::Undefined: return 5;

        // All numbers have same canonical type (compare by value)
        case BOONType::NumberDouble:
        case BOONType::NumberInt:
        case BOONType::NumberLong:
        case BOONType::NumberDecimal:
            return 10;

        case BOONType::String:
        case BOONType::Symbol:
            return 15;

        case BOONType::Object: return 20;

        // All array types have same canonical type
        case BOONType::Array:
        case BOONType::UniformArray:
        case BOONType::CompactArray:
            return 25;

        case BOONType::BinData: return 30;
        case BOONType::OID: return 35;
        case BOONType::Bool: return 40;
        case BOONType::Date: return 45;
        case BOONType::bsonTimestamp: return 47;
        case BOONType::RegEx: return 50;
        case BOONType::DBRef: return 55;
        case BOONType::Code: return 60;
        case BOONType::CodeWScope: return 65;
        case BOONType::SchemaRef: return 70;
        case BOONType::MaxKey: return 127;
        default: return -128;  // Invalid
    }
}

}  // namespace boon
}  // namespace mongo
