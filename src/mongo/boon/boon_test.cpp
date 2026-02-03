/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/boonobj.h"
#include "mongo/boon/boontypes.h"
#include "mongo/unittest/assert.h"
#include "mongo/unittest/framework.h"

namespace mongo {
namespace {

// ========== BOONType Tests ==========

TEST(BOONTypeTest, TypeNames) {
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::NumberDouble)), "double");
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::String)), "string");
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::Object)), "object");
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::Array)), "array");
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::UniformArray)), "uniformArray");
    ASSERT_EQ(std::string(boon::typeName(boon::BOONType::CompactArray)), "compactArray");
}

TEST(BOONTypeTest, IsBOONExtension) {
    ASSERT_FALSE(boon::isBOONExtension(boon::BOONType::Object));
    ASSERT_FALSE(boon::isBOONExtension(boon::BOONType::Array));
    ASSERT_FALSE(boon::isBOONExtension(boon::BOONType::String));

    ASSERT_TRUE(boon::isBOONExtension(boon::BOONType::UniformArray));
    ASSERT_TRUE(boon::isBOONExtension(boon::BOONType::CompactArray));
    ASSERT_TRUE(boon::isBOONExtension(boon::BOONType::SchemaRef));
}

TEST(BOONTypeTest, IsNumeric) {
    ASSERT_TRUE(boon::isNumericType(boon::BOONType::NumberDouble));
    ASSERT_TRUE(boon::isNumericType(boon::BOONType::NumberInt));
    ASSERT_TRUE(boon::isNumericType(boon::BOONType::NumberLong));
    ASSERT_TRUE(boon::isNumericType(boon::BOONType::NumberDecimal));

    ASSERT_FALSE(boon::isNumericType(boon::BOONType::String));
    ASSERT_FALSE(boon::isNumericType(boon::BOONType::Bool));
    ASSERT_FALSE(boon::isNumericType(boon::BOONType::Object));
}

TEST(BOONTypeTest, FixedSize) {
    ASSERT_TRUE(boon::isFixedSizeType(boon::BOONType::NumberDouble));
    ASSERT_EQ(boon::fixedSize(boon::BOONType::NumberDouble), 8u);

    ASSERT_TRUE(boon::isFixedSizeType(boon::BOONType::NumberInt));
    ASSERT_EQ(boon::fixedSize(boon::BOONType::NumberInt), 4u);

    ASSERT_TRUE(boon::isFixedSizeType(boon::BOONType::NumberLong));
    ASSERT_EQ(boon::fixedSize(boon::BOONType::NumberLong), 8u);

    ASSERT_TRUE(boon::isFixedSizeType(boon::BOONType::Bool));
    ASSERT_EQ(boon::fixedSize(boon::BOONType::Bool), 1u);

    ASSERT_FALSE(boon::isFixedSizeType(boon::BOONType::String));
    ASSERT_FALSE(boon::isFixedSizeType(boon::BOONType::Object));
    ASSERT_FALSE(boon::isFixedSizeType(boon::BOONType::Array));
}

TEST(BOONTypeTest, CanonicalType) {
    // All numeric types should have same canonical type
    ASSERT_EQ(boon::canonicalType(boon::BOONType::NumberDouble),
              boon::canonicalType(boon::BOONType::NumberInt));
    ASSERT_EQ(boon::canonicalType(boon::BOONType::NumberInt),
              boon::canonicalType(boon::BOONType::NumberLong));

    // All array types should have same canonical type
    ASSERT_EQ(boon::canonicalType(boon::BOONType::Array),
              boon::canonicalType(boon::BOONType::UniformArray));
    ASSERT_EQ(boon::canonicalType(boon::BOONType::Array),
              boon::canonicalType(boon::BOONType::CompactArray));

    // Different types should have different canonical types
    ASSERT_NE(boon::canonicalType(boon::BOONType::String),
              boon::canonicalType(boon::BOONType::NumberDouble));
    ASSERT_NE(boon::canonicalType(boon::BOONType::Object),
              boon::canonicalType(boon::BOONType::Array));
}

// ========== BOONObj Builder Tests ==========

TEST(BOONObjBuilderTest, EmptyObject) {
    BOONObjBuilder builder;
    BOONObj obj = builder.obj();

    ASSERT_TRUE(obj.isEmpty());
    ASSERT_EQ(obj.nFields(), 0);
    ASSERT_TRUE(obj.isValid());
}

TEST(BOONObjBuilderTest, SimpleTypes) {
    BOONObjBuilder builder;
    builder.append("double", 3.14);
    builder.append("string", "hello");
    builder.append("int", 42);
    builder.append("long", 123456789LL);
    builder.append("bool", true);
    builder.appendNull("null");

    BOONObj obj = builder.obj();

    ASSERT_FALSE(obj.isEmpty());
    ASSERT_EQ(obj.nFields(), 6);
    ASSERT_TRUE(obj.isValid());

    // Verify values
    ASSERT_EQ(obj["double"].Double(), 3.14);
    ASSERT_EQ(obj["string"].String(), "hello");
    ASSERT_EQ(obj["int"].Int(), 42);
    ASSERT_EQ(obj["long"].Long(), 123456789LL);
    ASSERT_EQ(obj["bool"].Bool(), true);
    ASSERT_EQ(obj["null"].boonType(), boon::BOONType::jstNULL);
}

TEST(BOONObjBuilderTest, NestedObject) {
    BOONObjBuilder innerBuilder;
    innerBuilder.append("x", 1);
    innerBuilder.append("y", 2);
    BOONObj inner = innerBuilder.obj();

    BOONObjBuilder outerBuilder;
    outerBuilder.append("name", "point");
    outerBuilder.append("coords", inner);

    BOONObj obj = outerBuilder.obj();

    ASSERT_EQ(obj.nFields(), 2);
    ASSERT_EQ(obj["name"].String(), "point");

    BOONObj coords = obj["coords"].Obj();
    ASSERT_EQ(coords["x"].Int(), 1);
    ASSERT_EQ(coords["y"].Int(), 2);
}

// ========== BOONObj Tests ==========

TEST(BOONObjTest, GetField) {
    BOONObjBuilder builder;
    builder.append("a", 1);
    builder.append("b", 2);
    builder.append("c", 3);
    BOONObj obj = builder.obj();

    ASSERT_EQ(obj["a"].Int(), 1);
    ASSERT_EQ(obj["b"].Int(), 2);
    ASSERT_EQ(obj["c"].Int(), 3);

    // Non-existent field returns EOO
    ASSERT_TRUE(obj["nonexistent"].eoo());
}

TEST(BOONObjTest, HasField) {
    BOONObjBuilder builder;
    builder.append("exists", 1);
    BOONObj obj = builder.obj();

    ASSERT_TRUE(obj.hasField("exists"));
    ASSERT_FALSE(obj.hasField("doesNotExist"));
}

TEST(BOONObjTest, Iteration) {
    BOONObjBuilder builder;
    builder.append("a", 1);
    builder.append("b", 2);
    builder.append("c", 3);
    BOONObj obj = builder.obj();

    std::vector<std::string> fields;
    std::vector<int> values;

    for (auto it = obj.begin(); it.more(); ) {
        BOONElement elem = it.next();
        fields.push_back(std::string(elem.fieldNameStringData()));
        values.push_back(elem.Int());
    }

    ASSERT_EQ(fields.size(), 3u);
    ASSERT_EQ(fields[0], "a");
    ASSERT_EQ(fields[1], "b");
    ASSERT_EQ(fields[2], "c");

    ASSERT_EQ(values[0], 1);
    ASSERT_EQ(values[1], 2);
    ASSERT_EQ(values[2], 3);
}

TEST(BOONObjTest, Comparison) {
    BOONObjBuilder b1;
    b1.append("x", 1);
    BOONObj obj1 = b1.obj();

    BOONObjBuilder b2;
    b2.append("x", 1);
    BOONObj obj2 = b2.obj();

    BOONObjBuilder b3;
    b3.append("x", 2);
    BOONObj obj3 = b3.obj();

    ASSERT_EQ(obj1.woCompare(obj2), 0);
    ASSERT_TRUE(obj1 == obj2);

    ASSERT_LT(obj1.woCompare(obj3), 0);
    ASSERT_TRUE(obj1 < obj3);
}

TEST(BOONObjTest, GetOwned) {
    BOONObjBuilder builder;
    builder.append("test", 42);
    BOONObj obj = builder.obj();

    ASSERT_TRUE(obj.isOwned());

    BOONObj copy = obj.getOwned();
    ASSERT_TRUE(copy.isOwned());
    ASSERT_EQ(obj["test"].Int(), copy["test"].Int());
}

// ========== BOONElement Tests ==========

TEST(BOONElementTest, TypeChecks) {
    BOONObjBuilder builder;
    builder.append("double", 3.14);
    builder.append("string", "hello");
    builder.append("int", 42);
    builder.append("bool", true);
    BOONObj obj = builder.obj();

    ASSERT_EQ(obj["double"].boonType(), boon::BOONType::NumberDouble);
    ASSERT_EQ(obj["string"].boonType(), boon::BOONType::String);
    ASSERT_EQ(obj["int"].boonType(), boon::BOONType::NumberInt);
    ASSERT_EQ(obj["bool"].boonType(), boon::BOONType::Bool);

    ASSERT_TRUE(obj["double"].isNumber());
    ASSERT_TRUE(obj["int"].isNumber());
    ASSERT_FALSE(obj["string"].isNumber());
    ASSERT_FALSE(obj["bool"].isNumber());
}

TEST(BOONElementTest, NumericConversions) {
    BOONObjBuilder builder;
    builder.append("int", 42);
    builder.append("long", 123456789LL);
    builder.append("double", 3.14);
    BOONObj obj = builder.obj();

    // All should be convertible to number()
    ASSERT_EQ(obj["int"].number(), 42.0);
    ASSERT_EQ(obj["long"].number(), 123456789.0);
    ASSERT_EQ(obj["double"].number(), 3.14);

    // safeNumberLong
    ASSERT_EQ(obj["int"].safeNumberLong(), 42LL);
    ASSERT_EQ(obj["long"].safeNumberLong(), 123456789LL);
    ASSERT_EQ(obj["double"].safeNumberLong(), 3LL);
}

TEST(BOONElementTest, FieldName) {
    BOONObjBuilder builder;
    builder.append("myField", 1);
    builder.append("anotherField", 2);
    BOONObj obj = builder.obj();

    ASSERT_EQ(obj["myField"].fieldNameStringData(), "myField");
    ASSERT_EQ(obj["anotherField"].fieldNameStringData(), "anotherField");
}

}  // namespace
}  // namespace mongo
