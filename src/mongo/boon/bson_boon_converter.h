/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#pragma once

#include "mongo/boon/boonobj.h"
#include "mongo/boon/uniform_array.h"
#include "mongo/bson/bsonobj.h"

#include <vector>

namespace mongo {
namespace boon {

/**
 * Options for BSON to BOON conversion.
 */
struct ConversionOptions {
    /**
     * Minimum array size to consider for uniform array conversion.
     * Arrays smaller than this will be kept as regular arrays.
     */
    size_t minUniformArraySize = 2;

    /**
     * Enable conversion of uniform arrays.
     * If false, arrays are kept in BSON format.
     */
    bool enableUniformArrays = true;

    /**
     * Enable conversion of primitive arrays to compact arrays.
     */
    bool enableCompactArrays = true;

    /**
     * Maximum depth for recursive conversion.
     * Prevents stack overflow on deeply nested documents.
     */
    int maxDepth = 100;
};

/**
 * Statistics from conversion.
 */
struct ConversionStats {
    int documentsConverted = 0;
    int uniformArraysCreated = 0;
    int compactArraysCreated = 0;
    int64_t bytesSaved = 0;  // Estimated
    int64_t originalSize = 0;
    int64_t convertedSize = 0;
};

/**
 * Convert BSON to BOON format.
 *
 * This function:
 * 1. Detects arrays of objects with identical schemas
 * 2. Converts them to UniformArrays for space savings
 * 3. Optionally converts primitive arrays to CompactArrays
 *
 * @param bson The BSON object to convert
 * @param options Conversion options
 * @param stats Optional stats output
 * @return The converted BOON object
 */
BOONObj bsonToBoon(const BSONObj& bson,
                   const ConversionOptions& options = ConversionOptions(),
                   ConversionStats* stats = nullptr);

/**
 * Convert BOON back to BSON format.
 *
 * This function:
 * 1. Expands UniformArrays back to regular arrays
 * 2. Expands CompactArrays back to regular arrays
 *
 * The result is a standard BSON document that can be used
 * with any BSON-compatible code.
 *
 * @param boon The BOON object to convert
 * @return The converted BSON object
 */
BSONObj boonToBson(const BOONObj& boon);

/**
 * Check if a BSON array is uniform (all elements have same schema).
 *
 * @param array The BSON array to check
 * @param schema Output: the detected schema (if uniform)
 * @return true if the array is uniform
 */
bool isUniformArray(const BSONObj& array, Schema* schema = nullptr);

/**
 * Check if a BSON array contains only primitives of the same type.
 *
 * @param array The BSON array to check
 * @param elementType Output: the type of elements (if compact)
 * @return true if the array can be converted to CompactArray
 */
bool isCompactArray(const BSONObj& array, BOONType* elementType = nullptr);

/**
 * Extract schema from a BSON object.
 *
 * @param obj The BSON object
 * @return The schema describing the object's structure
 */
Schema extractSchema(const BSONObj& obj);

/**
 * Convert a BSON array to UniformArray.
 *
 * @param array The BSON array (must be uniform)
 * @param schema The schema (from isUniformArray or extractSchema)
 * @return Binary data for the UniformArray
 */
std::vector<char> bsonArrayToUniformArray(const BSONObj& array,
                                          const Schema& schema);

/**
 * Convert a UniformArray back to BSON array.
 *
 * @param arr The UniformArray
 * @return BSON array with expanded elements
 */
BSONObj uniformArrayToBsonArray(const UniformArray& arr);

// ========== Inline Helpers ==========

/**
 * Map BSONType to BOONType.
 */
inline BOONType bsonTypeToBoonType(BSONType type) {
    // Direct mapping for compatible types
    return static_cast<BOONType>(static_cast<uint8_t>(type));
}

/**
 * Map BOONType to BSONType.
 * BOON extension types map to their closest BSON equivalent.
 */
inline BSONType boonTypeToBsonType(BOONType type) {
    switch (type) {
        case BOONType::UniformArray:
        case BOONType::CompactArray:
            return BSONType::Array;
        case BOONType::SchemaRef:
            return BSONType::Object;
        default:
            return static_cast<BSONType>(static_cast<uint8_t>(type));
    }
}

}  // namespace boon
}  // namespace mongo
