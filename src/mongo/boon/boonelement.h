/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#pragma once

#include "mongo/boon/boontypes.h"
#include "mongo/base/data_view.h"
#include "mongo/base/string_data.h"
#include "mongo/bson/bsontypes.h"
#include "mongo/bson/oid.h"
#include "mongo/bson/timestamp.h"
#include "mongo/platform/decimal128.h"

#include <cstdint>
#include <cstring>
#include <string>

namespace mongo {

// Forward declarations
class BOONObj;
class BOONElement;

namespace boon {
class UniformArray;
}

/**
 * BOONElement represents an element in a BOONObj.
 *
 * Like BSONElement, it's a view into the parent object's data.
 * The parent BOONObj must stay in scope for the life of the BOONElement.
 *
 * Binary layout:
 *   <type:1><fieldName:cstring><value:variable>
 *
 * For UniformArray type:
 *   <type:1><fieldName:cstring><schema><rowCount:4><rows...>
 */
class BOONElement {
public:
    /**
     * Default constructor creates an EOO (End Of Object) element.
     */
    BOONElement() : _data(kEooElement) {}

    /**
     * Construct from raw data pointer.
     * @param data Pointer to element data (type byte)
     */
    explicit BOONElement(const char* data) : _data(data) {}

    /**
     * Construct with known field name size (optimization).
     */
    BOONElement(const char* data, int fieldNameSize)
        : _data(data), _fieldNameSize(fieldNameSize) {}

    // ========== Type Accessors ==========

    /**
     * Get the BOON type of this element.
     */
    boon::BOONType boonType() const {
        return static_cast<boon::BOONType>(*reinterpret_cast<const uint8_t*>(_data));
    }

    /**
     * Get the BSON type (for compatibility).
     * BOON extension types return BSONType::EOO.
     */
    BSONType type() const {
        uint8_t t = static_cast<uint8_t>(boonType());
        if (t > 0x13 && t < 0x7F) {
            // BOON extension type - no direct BSON equivalent
            // For arrays, we can map to Array
            if (t == static_cast<uint8_t>(boon::BOONType::UniformArray) ||
                t == static_cast<uint8_t>(boon::BOONType::CompactArray)) {
                return BSONType::Array;
            }
            return BSONType::EOO;
        }
        return static_cast<BSONType>(t);
    }

    /**
     * Check if this is an EOO element.
     */
    bool eoo() const {
        return boonType() == boon::BOONType::EOO;
    }

    /**
     * Check if this is a BOON extension type.
     */
    bool isBOONExtension() const {
        return boon::isBOONExtension(boonType());
    }

    /**
     * Check if this is a UniformArray.
     */
    bool isUniformArray() const {
        return boonType() == boon::BOONType::UniformArray;
    }

    /**
     * Check if this is a CompactArray.
     */
    bool isCompactArray() const {
        return boonType() == boon::BOONType::CompactArray;
    }

    /**
     * Check if this is any kind of array (including BOON arrays).
     */
    bool isAnyArray() const {
        auto t = boonType();
        return t == boon::BOONType::Array ||
               t == boon::BOONType::UniformArray ||
               t == boon::BOONType::CompactArray;
    }

    // ========== Field Name ==========

    /**
     * Get the field name.
     */
    StringData fieldNameStringData() const {
        if (_fieldNameSize < 0) {
            _fieldNameSize = strlen(_data + 1);
        }
        return StringData(_data + 1, _fieldNameSize);
    }

    /**
     * Get field name as C string.
     */
    const char* fieldName() const {
        return _data + 1;
    }

    /**
     * Get field name size (excluding null terminator).
     */
    int fieldNameSize() const {
        if (_fieldNameSize < 0) {
            _fieldNameSize = strlen(_data + 1);
        }
        return _fieldNameSize;
    }

    // ========== Value Accessors ==========

    /**
     * Get pointer to value data (after type and field name).
     */
    const char* value() const {
        return _data + 1 + fieldNameSize() + 1;  // type + name + null
    }

    /**
     * Get total size of this element.
     */
    int size() const;

    /**
     * Get size of value only.
     */
    int valueSize() const;

    // ========== Type-specific Value Getters ==========

    double Double() const {
        return ConstDataView(value()).read<LittleEndian<double>>();
    }

    StringData valueStringData() const {
        int len = ConstDataView(value()).read<LittleEndian<int32_t>>();
        return StringData(value() + 4, len - 1);  // -1 for null terminator
    }

    std::string String() const {
        return std::string(valueStringData());
    }

    int32_t Int() const {
        return ConstDataView(value()).read<LittleEndian<int32_t>>();
    }

    int64_t Long() const {
        return ConstDataView(value()).read<LittleEndian<int64_t>>();
    }

    bool Bool() const {
        return *value() != 0;
    }

    OID OID() const {
        return mongo::OID::from(value());
    }

    Date_t Date() const {
        return Date_t::fromMillisSinceEpoch(Long());
    }

    Timestamp timestamp() const {
        return Timestamp(ConstDataView(value()).read<LittleEndian<uint64_t>>());
    }

    Decimal128 Decimal() const {
        uint64_t low = ConstDataView(value()).read<LittleEndian<uint64_t>>();
        uint64_t high = ConstDataView(value() + 8).read<LittleEndian<uint64_t>>();
        return Decimal128(Decimal128::Value{low, high});
    }

    /**
     * Get embedded object (for Object type).
     */
    BOONObj Obj() const;

    /**
     * Get array as BOONObj (for Array type).
     */
    BOONObj Array() const;

    /**
     * Get UniformArray (for UniformArray type).
     */
    boon::UniformArray uniformArray() const;

    // ========== Numeric Conversions ==========

    bool isNumber() const {
        return boon::isNumericType(boonType());
    }

    double number() const {
        switch (boonType()) {
            case boon::BOONType::NumberDouble: return Double();
            case boon::BOONType::NumberInt: return static_cast<double>(Int());
            case boon::BOONType::NumberLong: return static_cast<double>(Long());
            case boon::BOONType::NumberDecimal: return Decimal().toDouble();
            default: return 0;
        }
    }

    int64_t safeNumberLong() const {
        switch (boonType()) {
            case boon::BOONType::NumberDouble: {
                double d = Double();
                if (std::isnan(d)) return 0;
                if (d > static_cast<double>(std::numeric_limits<int64_t>::max()))
                    return std::numeric_limits<int64_t>::max();
                if (d < static_cast<double>(std::numeric_limits<int64_t>::min()))
                    return std::numeric_limits<int64_t>::min();
                return static_cast<int64_t>(d);
            }
            case boon::BOONType::NumberInt: return Int();
            case boon::BOONType::NumberLong: return Long();
            case boon::BOONType::NumberDecimal: return Decimal().toLong();
            default: return 0;
        }
    }

    // ========== Comparison ==========

    /**
     * Get raw data pointer.
     */
    const char* rawdata() const {
        return _data;
    }

    /**
     * Compare two elements.
     */
    int woCompare(const BOONElement& other, bool considerFieldName = true) const;

    // ========== Operators ==========

    bool operator==(const BOONElement& other) const {
        return woCompare(other, true) == 0;
    }

    bool operator!=(const BOONElement& other) const {
        return !(*this == other);
    }

    explicit operator bool() const {
        return !eoo();
    }

private:
    const char* _data;
    mutable int _fieldNameSize = -1;  // Cached, -1 means not computed

    // EOO element singleton
    static constexpr char kEooElement[1] = {0};
};

}  // namespace mongo
