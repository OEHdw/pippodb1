/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#pragma once

#include "mongo/boon/boonelement.h"
#include "mongo/boon/boontypes.h"
#include "mongo/base/string_data.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mongo {

// Forward declarations
class BSONObj;

/**
 * BOONObj represents a BOON document.
 *
 * Like BSONObj, it's an immutable view over binary data.
 * The data is reference-counted for efficient copying.
 *
 * Binary layout (same as BSON for compatibility):
 *   <totalSize:4><elements...><EOO:1>
 *
 * Where each element is:
 *   <type:1><fieldName:cstring><value:variable>
 *
 * The difference from BSON is that BOON supports additional types:
 * - UniformArray (0x14): Efficient storage for arrays of uniform objects
 * - CompactArray (0x15): Efficient storage for arrays of primitives
 * - SchemaRef (0x16): Reference to external schema
 */
class BOONObj {
public:
    /**
     * Iterator over BOONObj elements.
     */
    class Iterator {
    public:
        explicit Iterator(const char* data) : _pos(data) {}

        BOONElement operator*() const {
            return BOONElement(_pos);
        }

        Iterator& operator++() {
            BOONElement elem(_pos);
            _pos += elem.size();
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return _pos != other._pos;
        }

        bool more() const {
            return *_pos != 0;  // Not EOO
        }

        BOONElement next() {
            BOONElement elem(_pos);
            _pos += elem.size();
            return elem;
        }

    private:
        const char* _pos;
    };

    /**
     * Default constructor creates empty object {}.
     */
    BOONObj();

    /**
     * Construct from raw data.
     * @param data Pointer to BOON binary data (starting with size)
     */
    explicit BOONObj(const char* data);

    /**
     * Construct from shared buffer.
     */
    BOONObj(std::shared_ptr<const char[]> buffer, const char* data);

    /**
     * Copy constructor (shares underlying data).
     */
    BOONObj(const BOONObj& other) = default;

    /**
     * Move constructor.
     */
    BOONObj(BOONObj&& other) = default;

    /**
     * Assignment operators.
     */
    BOONObj& operator=(const BOONObj& other) = default;
    BOONObj& operator=(BOONObj&& other) = default;

    // ========== Size and Validity ==========

    /**
     * Get total size in bytes (including size prefix and EOO).
     */
    int objsize() const {
        return ConstDataView(_data).read<LittleEndian<int32_t>>();
    }

    /**
     * Check if this is an empty object {}.
     */
    bool isEmpty() const {
        return objsize() <= 5;  // 4 bytes size + 1 byte EOO
    }

    /**
     * Get number of top-level fields.
     */
    int nFields() const;

    /**
     * Check if object is valid.
     */
    bool isValid() const;

    // ========== Element Access ==========

    /**
     * Get element by field name.
     * Returns EOO element if not found.
     */
    BOONElement operator[](StringData field) const {
        return getField(field);
    }

    /**
     * Get element by field name.
     */
    BOONElement getField(StringData field) const;

    /**
     * Check if field exists.
     */
    bool hasField(StringData field) const {
        return !getField(field).eoo();
    }

    /**
     * Get first element.
     */
    BOONElement firstElement() const {
        return BOONElement(_data + 4);
    }

    // ========== Iteration ==========

    /**
     * Begin iterator.
     */
    Iterator begin() const {
        return Iterator(_data + 4);
    }

    /**
     * End iterator (points to EOO).
     */
    Iterator end() const {
        return Iterator(_data + objsize() - 1);
    }

    // ========== Raw Data Access ==========

    /**
     * Get raw data pointer.
     */
    const char* objdata() const {
        return _data;
    }

    /**
     * Check if owns the data buffer.
     */
    bool isOwned() const {
        return _buffer != nullptr;
    }

    /**
     * Make a copy that owns its data.
     */
    BOONObj getOwned() const;

    // ========== Conversion ==========

    /**
     * Convert to BSONObj (expands UniformArrays to regular arrays).
     */
    BSONObj toBSON() const;

    /**
     * Create BOONObj from BSONObj (detects and converts uniform arrays).
     */
    static BOONObj fromBSON(const BSONObj& bson);

    // ========== Comparison ==========

    /**
     * Compare with another BOONObj.
     */
    int woCompare(const BOONObj& other) const;

    bool operator==(const BOONObj& other) const {
        return woCompare(other) == 0;
    }

    bool operator!=(const BOONObj& other) const {
        return !(*this == other);
    }

    bool operator<(const BOONObj& other) const {
        return woCompare(other) < 0;
    }

    // ========== Debugging ==========

    /**
     * Convert to JSON string.
     */
    std::string toString() const;

    /**
     * Get BOON-specific stats (for debugging/analysis).
     */
    struct Stats {
        int totalSize;
        int numFields;
        int numUniformArrays;
        int numCompactArrays;
        int savedBytes;  // Estimated bytes saved vs BSON
    };

    Stats getStats() const;

private:
    const char* _data;
    std::shared_ptr<const char[]> _buffer;  // Owned buffer (if any)

    // Empty object data
    static constexpr char kEmptyObject[5] = {5, 0, 0, 0, 0};
};

/**
 * BOONObjBuilder - Build BOONObj incrementally.
 */
class BOONObjBuilder {
public:
    BOONObjBuilder();

    /**
     * Append a double value.
     */
    BOONObjBuilder& append(StringData field, double val);

    /**
     * Append a string value.
     */
    BOONObjBuilder& append(StringData field, StringData val);

    /**
     * Append an int32 value.
     */
    BOONObjBuilder& append(StringData field, int32_t val);

    /**
     * Append an int64 value.
     */
    BOONObjBuilder& append(StringData field, int64_t val);

    /**
     * Append a bool value.
     */
    BOONObjBuilder& append(StringData field, bool val);

    /**
     * Append null.
     */
    BOONObjBuilder& appendNull(StringData field);

    /**
     * Append an OID.
     */
    BOONObjBuilder& append(StringData field, const OID& oid);

    /**
     * Append a nested object.
     */
    BOONObjBuilder& append(StringData field, const BOONObj& obj);

    /**
     * Append a UniformArray.
     */
    BOONObjBuilder& appendUniformArray(StringData field,
                                        const boon::UniformArray& arr);

    /**
     * Start building a nested object.
     */
    BOONObjBuilder& subobjStart(StringData field);

    /**
     * Start building an array.
     */
    BOONObjBuilder& subarrayStart(StringData field);

    /**
     * Get number of fields appended.
     */
    int numFields() const {
        return _numFields;
    }

    /**
     * Build the final BOONObj.
     */
    BOONObj obj();

    /**
     * Build and reset for reuse.
     */
    BOONObj done() {
        return obj();
    }

private:
    std::vector<char> _buffer;
    int _numFields = 0;

    void appendType(boon::BOONType type);
    void appendFieldName(StringData field);
    void appendBytes(const char* data, size_t len);

    template <typename T>
    void appendNum(T val);
};

// Inline implementation of BOONElement methods that need BOONObj

inline BOONObj BOONElement::Obj() const {
    return BOONObj(value());
}

inline BOONObj BOONElement::Array() const {
    return BOONObj(value());
}

}  // namespace mongo
