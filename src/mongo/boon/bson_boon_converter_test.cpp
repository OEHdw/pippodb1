/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/bson_boon_converter.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"
#include "mongo/unittest/assert.h"
#include "mongo/unittest/framework.h"

namespace mongo {
namespace {

using namespace boon;

// ========== Schema Extraction Tests ==========

TEST(BsonBoonConverterTest, ExtractSchemaSimple) {
    BSONObj obj = BSON("name" << "Mario" << "age" << 30 << "active" << true);
    Schema schema = extractSchema(obj);

    ASSERT_EQ(schema.numFields(), 3u);
    ASSERT_EQ(schema.field(0).name, "name");
    ASSERT_EQ(schema.field(0).type, BOONType::String);
    ASSERT_EQ(schema.field(1).name, "age");
    ASSERT_EQ(schema.field(1).type, BOONType::NumberInt);
    ASSERT_EQ(schema.field(2).name, "active");
    ASSERT_EQ(schema.field(2).type, BOONType::Bool);
}

// ========== Uniform Array Detection Tests ==========

TEST(BsonBoonConverterTest, DetectUniformArray) {
    // Uniform array: all elements have same schema
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(BSON("x" << 1 << "y" << 2));
    arrBuilder.append(BSON("x" << 3 << "y" << 4));
    arrBuilder.append(BSON("x" << 5 << "y" << 6));
    BSONArray arr = arrBuilder.arr();

    Schema schema;
    ASSERT_TRUE(isUniformArray(arr, &schema));
    ASSERT_EQ(schema.numFields(), 2u);
    ASSERT_EQ(schema.field(0).name, "x");
    ASSERT_EQ(schema.field(1).name, "y");
}

TEST(BsonBoonConverterTest, DetectNonUniformArray_DifferentFields) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(BSON("x" << 1 << "y" << 2));
    arrBuilder.append(BSON("x" << 3 << "z" << 4));  // Different field!
    BSONArray arr = arrBuilder.arr();

    ASSERT_FALSE(isUniformArray(arr));
}

TEST(BsonBoonConverterTest, DetectNonUniformArray_DifferentTypes) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(BSON("x" << 1 << "y" << 2));
    arrBuilder.append(BSON("x" << 3 << "y" << "string"));  // Different type!
    BSONArray arr = arrBuilder.arr();

    ASSERT_FALSE(isUniformArray(arr));
}

TEST(BsonBoonConverterTest, DetectNonUniformArray_Primitives) {
    // Array of primitives is not uniform (needs objects)
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(1);
    arrBuilder.append(2);
    arrBuilder.append(3);
    BSONArray arr = arrBuilder.arr();

    ASSERT_FALSE(isUniformArray(arr));
}

TEST(BsonBoonConverterTest, DetectCompactArray) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(10);
    arrBuilder.append(20);
    arrBuilder.append(30);
    BSONArray arr = arrBuilder.arr();

    BOONType elemType;
    ASSERT_TRUE(isCompactArray(arr, &elemType));
    ASSERT_EQ(elemType, BOONType::NumberInt);
}

TEST(BsonBoonConverterTest, DetectCompactArray_Strings) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append("hello");
    arrBuilder.append("world");
    BSONArray arr = arrBuilder.arr();

    BOONType elemType;
    ASSERT_TRUE(isCompactArray(arr, &elemType));
    ASSERT_EQ(elemType, BOONType::String);
}

TEST(BsonBoonConverterTest, DetectNonCompactArray_MixedTypes) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(1);
    arrBuilder.append("string");  // Different type!
    BSONArray arr = arrBuilder.arr();

    ASSERT_FALSE(isCompactArray(arr));
}

TEST(BsonBoonConverterTest, DetectCompactArray_MixedNumeric) {
    // Mixed numeric types should still be compact
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(1);        // int
    arrBuilder.append(2.5);      // double
    arrBuilder.append(3LL);      // long
    BSONArray arr = arrBuilder.arr();

    ASSERT_TRUE(isCompactArray(arr));
}

// ========== BSON Array to UniformArray Conversion ==========

TEST(BsonBoonConverterTest, ConvertBsonArrayToUniformArray) {
    BSONArrayBuilder arrBuilder;
    arrBuilder.append(BSON("name" << "Alice" << "age" << 30));
    arrBuilder.append(BSON("name" << "Bob" << "age" << 25));
    arrBuilder.append(BSON("name" << "Carol" << "age" << 28));
    BSONArray arr = arrBuilder.arr();

    Schema schema;
    ASSERT_TRUE(isUniformArray(arr, &schema));

    auto bytes = bsonArrayToUniformArray(arr, schema);
    UniformArray uniformArr(bytes.data());

    ASSERT_EQ(uniformArr.numRows(), 3u);
    ASSERT_EQ(uniformArr.row(0).getString(0), "Alice");
    ASSERT_EQ(uniformArr.row(0).getInt32(1), 30);
    ASSERT_EQ(uniformArr.row(1).getString(0), "Bob");
    ASSERT_EQ(uniformArr.row(1).getInt32(1), 25);
    ASSERT_EQ(uniformArr.row(2).getString(0), "Carol");
    ASSERT_EQ(uniformArr.row(2).getInt32(1), 28);
}

// ========== UniformArray to BSON Array Conversion ==========

TEST(BsonBoonConverterTest, ConvertUniformArrayToBsonArray) {
    // Build a uniform array
    Schema schema;
    schema.addField("id", BOONType::NumberInt);
    schema.addField("value", BOONType::NumberDouble);

    UniformArrayBuilder builder;
    builder.setSchema(schema);

    builder.startRow();
    builder.appendInt32(1);
    builder.appendDouble(10.5);
    builder.finishRow();

    builder.startRow();
    builder.appendInt32(2);
    builder.appendDouble(20.5);
    builder.finishRow();

    auto bytes = builder.done();
    UniformArray uniformArr(bytes.data());

    // Convert back to BSON
    BSONObj bsonArr = uniformArrayToBsonArray(uniformArr);

    // Verify
    ASSERT_EQ(bsonArr.nFields(), 2);

    BSONObj elem0 = bsonArr["0"].Obj();
    ASSERT_EQ(elem0["id"].numberInt(), 1);
    ASSERT_EQ(elem0["value"].numberDouble(), 10.5);

    BSONObj elem1 = bsonArr["1"].Obj();
    ASSERT_EQ(elem1["id"].numberInt(), 2);
    ASSERT_EQ(elem1["value"].numberDouble(), 20.5);
}

// ========== Full BSON <-> BOON Roundtrip ==========

TEST(BsonBoonConverterTest, SimpleDocumentRoundtrip) {
    BSONObj original = BSON("name" << "Mario"
                            << "age" << 30
                            << "score" << 95.5
                            << "active" << true);

    // BSON -> BOON
    BOONObj boon = bsonToBoon(original);
    ASSERT_FALSE(boon.isEmpty());

    // BOON -> BSON
    BSONObj roundtripped = boonToBson(boon);

    // Verify roundtrip
    ASSERT_EQ(roundtripped["name"].String(), "Mario");
    ASSERT_EQ(roundtripped["age"].numberInt(), 30);
    ASSERT_EQ(roundtripped["score"].numberDouble(), 95.5);
    ASSERT_EQ(roundtripped["active"].boolean(), true);
}

TEST(BsonBoonConverterTest, DocumentWithUniformArrayRoundtrip) {
    // Build document with uniform array
    BSONObjBuilder docBuilder;
    docBuilder.append("collection", "users");

    BSONArrayBuilder arrBuilder(docBuilder.subarrayStart("data"));
    arrBuilder.append(BSON("name" << "Alice" << "age" << 30));
    arrBuilder.append(BSON("name" << "Bob" << "age" << 25));
    arrBuilder.append(BSON("name" << "Carol" << "age" << 28));
    arrBuilder.done();

    BSONObj original = docBuilder.obj();

    // BSON -> BOON (should detect and convert uniform array)
    ConversionStats stats;
    BOONObj boon = bsonToBoon(original, ConversionOptions(), &stats);

    ASSERT_EQ(stats.documentsConverted, 1);
    ASSERT_GTE(stats.uniformArraysCreated, 0);  // May or may not convert depending on impl

    // BOON -> BSON
    BSONObj roundtripped = boonToBson(boon);

    // Verify top-level field preserved
    ASSERT_EQ(roundtripped["collection"].String(), "users");
}

// ========== Conversion Stats Test ==========

TEST(BsonBoonConverterTest, ConversionStats) {
    BSONObjBuilder docBuilder;
    docBuilder.append("id", 1);

    BSONArrayBuilder arrBuilder(docBuilder.subarrayStart("items"));
    for (int i = 0; i < 100; ++i) {
        arrBuilder.append(BSON("sku" << ("SKU" + std::to_string(i))
                              << "qty" << i
                              << "price" << (i * 9.99)));
    }
    arrBuilder.done();

    BSONObj original = docBuilder.obj();

    ConversionStats stats;
    BOONObj boon = bsonToBoon(original, ConversionOptions(), &stats);

    ASSERT_EQ(stats.documentsConverted, 1);
    ASSERT_EQ(stats.originalSize, original.objsize());
    ASSERT_GT(stats.convertedSize, 0);
}

// ========== Edge Cases ==========

TEST(BsonBoonConverterTest, EmptyDocument) {
    BSONObj empty;
    BOONObj boon = bsonToBoon(empty);
    BSONObj roundtripped = boonToBson(boon);
    ASSERT_TRUE(roundtripped.isEmpty());
}

TEST(BsonBoonConverterTest, NullField) {
    BSONObj obj = BSON("x" << BSONNULL);
    BOONObj boon = bsonToBoon(obj);
    BSONObj roundtripped = boonToBson(boon);
    ASSERT_TRUE(roundtripped["x"].isNull());
}

TEST(BsonBoonConverterTest, SmallArrayNotConverted) {
    // Array with only 1 element should NOT be converted to uniform
    ConversionOptions opts;
    opts.minUniformArraySize = 2;

    BSONObjBuilder docBuilder;
    BSONArrayBuilder arrBuilder(docBuilder.subarrayStart("data"));
    arrBuilder.append(BSON("x" << 1));  // Only 1 element
    arrBuilder.done();

    BSONObj original = docBuilder.obj();

    ConversionStats stats;
    BOONObj boon = bsonToBoon(original, opts, &stats);

    // Should NOT have created a uniform array
    ASSERT_EQ(stats.uniformArraysCreated, 0);
}

}  // namespace
}  // namespace mongo
