/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/boonobj.h"
#include "mongo/boon/bson_boon_converter.h"
#include "mongo/boon/uniform_array.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"

#include <benchmark/benchmark.h>

namespace mongo {
namespace {

// ========== Helper: Generate BSON with uniform array ==========

BSONObj makeUniformArrayBson(int numElements) {
    BSONObjBuilder docBuilder;
    docBuilder.append("collection", "users");

    BSONArrayBuilder arrBuilder(docBuilder.subarrayStart("data"));
    for (int i = 0; i < numElements; ++i) {
        arrBuilder.append(BSON("name" << ("User" + std::to_string(i))
                              << "age" << (20 + i % 50)
                              << "score" << (i * 1.5)
                              << "active" << (i % 2 == 0)));
    }
    arrBuilder.done();

    return docBuilder.obj();
}

// ========== Benchmark: BSON Size vs BOON Size ==========

static void BM_BsonSize(benchmark::State& state) {
    int n = state.range(0);
    BSONObj bson = makeUniformArrayBson(n);

    for (auto _ : state) {
        benchmark::DoNotOptimize(bson.objsize());
    }

    state.counters["bytes"] = bson.objsize();
}
BENCHMARK(BM_BsonSize)->Arg(10)->Arg(100)->Arg(1000);

static void BM_BoonSize(benchmark::State& state) {
    int n = state.range(0);
    BSONObj bson = makeUniformArrayBson(n);
    boon::BOONObj boon = boon::bsonToBoon(bson);

    for (auto _ : state) {
        benchmark::DoNotOptimize(boon.objsize());
    }

    state.counters["bytes"] = boon.objsize();
    state.counters["savings_pct"] =
        100.0 * (1.0 - static_cast<double>(boon.objsize()) / bson.objsize());
}
BENCHMARK(BM_BoonSize)->Arg(10)->Arg(100)->Arg(1000);

// ========== Benchmark: Conversion Speed ==========

static void BM_BsonToBoon(benchmark::State& state) {
    int n = state.range(0);
    BSONObj bson = makeUniformArrayBson(n);

    for (auto _ : state) {
        boon::BOONObj boon = boon::bsonToBoon(bson);
        benchmark::DoNotOptimize(boon);
    }

    state.SetBytesProcessed(state.iterations() * bson.objsize());
}
BENCHMARK(BM_BsonToBoon)->Arg(10)->Arg(100)->Arg(1000);

static void BM_BoonToBson(benchmark::State& state) {
    int n = state.range(0);
    BSONObj bson = makeUniformArrayBson(n);
    boon::BOONObj boon = boon::bsonToBoon(bson);

    for (auto _ : state) {
        BSONObj result = boon::boonToBson(boon);
        benchmark::DoNotOptimize(result);
    }

    state.SetBytesProcessed(state.iterations() * boon.objsize());
}
BENCHMARK(BM_BoonToBson)->Arg(10)->Arg(100)->Arg(1000);

// ========== Benchmark: UniformArray Row Access ==========

static void BM_UniformArrayRowAccess(benchmark::State& state) {
    int n = state.range(0);

    boon::Schema schema;
    schema.addField("id", boon::BOONType::NumberInt);
    schema.addField("value", boon::BOONType::NumberDouble);

    boon::UniformArrayBuilder builder;
    builder.setSchema(schema);

    for (int i = 0; i < n; ++i) {
        builder.startRow();
        builder.appendInt32(i);
        builder.appendDouble(i * 1.5);
        builder.finishRow();
    }

    auto bytes = builder.done();
    boon::UniformArray arr(bytes.data());

    for (auto _ : state) {
        double sum = 0;
        for (size_t i = 0; i < arr.numRows(); ++i) {
            sum += arr.row(i).getDouble(1);
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_UniformArrayRowAccess)->Arg(10)->Arg(100)->Arg(1000);

// ========== Benchmark: BSON Array Element Access (comparison) ==========

static void BM_BsonArrayAccess(benchmark::State& state) {
    int n = state.range(0);

    BSONArrayBuilder arrBuilder;
    for (int i = 0; i < n; ++i) {
        arrBuilder.append(BSON("id" << i << "value" << (i * 1.5)));
    }
    BSONArray arr = arrBuilder.arr();

    for (auto _ : state) {
        double sum = 0;
        for (const auto& elem : arr) {
            sum += elem.Obj()["value"].numberDouble();
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_BsonArrayAccess)->Arg(10)->Arg(100)->Arg(1000);

// ========== Benchmark: BOONObj Builder ==========

static void BM_BoonObjBuilder(benchmark::State& state) {
    for (auto _ : state) {
        BOONObjBuilder builder;
        builder.append("name", "testUser");
        builder.append("age", 30);
        builder.append("score", 95.5);
        builder.append("active", true);
        BOONObj obj = builder.obj();
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(BM_BoonObjBuilder);

static void BM_BsonObjBuilder(benchmark::State& state) {
    for (auto _ : state) {
        BSONObj obj = BSON("name" << "testUser"
                          << "age" << 30
                          << "score" << 95.5
                          << "active" << true);
        benchmark::DoNotOptimize(obj);
    }
}
BENCHMARK(BM_BsonObjBuilder);

}  // namespace
}  // namespace mongo
