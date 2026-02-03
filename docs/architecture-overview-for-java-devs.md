# Architettura MongoDB Server — Guida per sviluppatori Java/MVC

## Premessa: cosa NON è questo progetto

Venendo da Java web con Spring MVC, il primo istinto è cercare Controller, Service, Repository.
Qui non li trovi. MongoDB Server è un **database engine in C++**, non un'applicazione web.
Ma i concetti architetturali di separazione a strati ci sono eccome — solo con nomi diversi.

---

## Mappa mentale: da MVC a MongoDB Server

| Concetto Java/MVC          | Equivalente in MongoDB Server        | Dove si trova                          |
|-----------------------------|--------------------------------------|----------------------------------------|
| `DispatcherServlet`         | `ServiceEntryPoint`                  | `src/mongo/transport/`                 |
| Controller                  | `Command` (es. `FindCmd`, `InsertCmd`) | `src/mongo/db/commands/`             |
| Service Layer               | Query engine + Pipeline              | `src/mongo/db/query/`, `db/pipeline/` |
| Repository / DAO            | `RecordStore`, `SortedDataInterface` | `src/mongo/db/storage/`               |
| `HttpServletRequest`        | `OperationContext`                   | `src/mongo/db/operation_context.h`     |
| `ApplicationContext` (Spring)| `ServiceContext`                    | `src/mongo/db/service_context.h`      |
| JPA/Hibernate               | WiredTiger (storage engine)          | `src/mongo/db/storage/wiredtiger/`    |
| DTO / JSON mapping          | BSON (Binary JSON)                   | `src/mongo/bson/`                      |
| Interceptor / Filter        | `OpObserver`                         | `src/mongo/db/op_observer/`           |
| `@Transactional`            | `WriteUnitOfWork`                    | `src/mongo/db/storage/`               |

---

## La struttura a strati

```
┌─────────────────────────────────────────────────────┐
│                    RETE / PROTOCOLLO                 │
│         Transport Layer (TCP, gRPC, TLS)            │
│              src/mongo/transport/                    │
├─────────────────────────────────────────────────────┤
│                  DISPATCH (≈ Controller)             │
│       ServiceEntryPoint → SessionWorkflow           │
│    Riceve la richiesta, la smista al Command giusto  │
├─────────────────────────────────────────────────────┤
│                  COMANDI (≈ Controller)              │
│         FindCmd, InsertCmd, AggregateCmd ...         │
│              src/mongo/db/commands/                  │
├─────────────────────────────────────────────────────┤
│               QUERY ENGINE (≈ Service)              │
│     Parsing → Ottimizzazione → Piano esecutivo      │
│  src/mongo/db/query/  +  src/mongo/db/pipeline/     │
├─────────────────────────────────────────────────────┤
│               ESECUZIONE (≈ Service)                │
│        Classic Executor  oppure  SBE Executor       │
│              src/mongo/db/exec/                      │
├─────────────────────────────────────────────────────┤
│              STORAGE API (≈ Repository)              │
│     RecordStore, SortedDataInterface, KVEngine       │
│              src/mongo/db/storage/                   │
├─────────────────────────────────────────────────────┤
│            STORAGE ENGINE (≈ Database)               │
│                    WiredTiger                        │
│         src/mongo/db/storage/wiredtiger/            │
└─────────────────────────────────────────────────────┘
```

---

## Il ciclo di vita di una richiesta (come il request lifecycle di Spring)

### Lettura (`find`)

```
Client invia query via TCP
  → Transport Layer accetta la connessione (≈ Tomcat accept)
    → SessionWorkflow gestisce il messaggio (≈ DispatcherServlet)
      → ServiceEntryPoint.handleRequest() smista al comando
        → FindCmd.run() (≈ @GetMapping handler)
          → OperationContext creato (≈ new HttpServletRequest)
            → Query parsing: BSON → CanonicalQuery
              → Query Planner genera N piani di esecuzione
                → Plan Ranker sceglie il migliore (cost-based)
                  → PlanExecutor esegue (Classic o SBE)
                    → RecordStore.getCursor() legge dallo storage
                      → Risultati filtrati dal Matcher
                        → Stream di documenti BSON al client
```

### Scrittura (`insert`)

```
Client invia insert
  → InsertCmd.run()
    → WriteUnitOfWork aperta (≈ @Transactional begin)
      → RecordStore.insertRecord() scrive nel WiredTiger
      → OpObserver notificato (≈ Event Listener)
        → Scrive entry nell'OpLog (per la replicazione)
    → WriteUnitOfWork commit (≈ @Transactional commit)
      → Risposta al client
```

---

## Le directory principali — cosa c'è dove

### `src/mongo/db/` — Il cuore del database

Questa è la directory più importante. È come `src/main/java` nel tuo progetto Spring.

| Directory            | Cosa fa                                                    | Analogia Java                    |
|----------------------|------------------------------------------------------------|----------------------------------|
| `commands/`          | Implementa tutti i comandi (find, insert, update, ...)     | `@RestController` handlers       |
| `query/`             | Parsing, ottimizzazione e pianificazione query              | Service layer per le query       |
| `exec/`              | Motori di esecuzione (Classic e SBE)                       | Service layer (esecuzione)       |
| `pipeline/`          | Aggregation pipeline ($match, $group, $lookup, ...)        | Stream processing / chain of responsibility |
| `matcher/`           | Valutazione filtri (`{age: {$gt: 18}}`)                    | `Specification` pattern          |
| `storage/`           | Astrazione storage engine                                  | Repository / DAO interfaces      |
| `storage/wiredtiger/`| Implementazione concreta WiredTiger                        | JPA implementation               |
| `repl/`              | Replica set, oplog, sincronizzazione primario/secondario   | (nessun equivalente diretto)     |
| `catalog/`           | Metadati di collezioni e indici                            | Schema/metadata management       |
| `index/`             | Gestione indici                                            | Index annotations                |
| `transaction/`       | Transazioni multi-documento                                | `TransactionManager`             |
| `auth/`              | Autenticazione e autorizzazione                            | Spring Security                  |
| `op_observer/`       | Notifiche su operazioni di scrittura                       | `@EventListener` / AOP           |
| `s/`                 | Sharding (distribuzione dati su più nodi)                  | (nessun equivalente)             |

### Altre directory importanti

| Directory              | Cosa fa                                        |
|------------------------|------------------------------------------------|
| `src/mongo/bson/`      | Serializzazione BSON (il formato dati di MongoDB) |
| `src/mongo/transport/` | Layer di rete, gestione connessioni             |
| `src/mongo/executor/`  | Framework asincrono (Future/Promise, thread pool) |
| `src/mongo/util/`      | Utility varie, cancellation tokens, concurrency |
| `src/mongo/client/`    | Client interno per comunicazione tra nodi       |
| `jstests/`             | Test di integrazione in JavaScript              |
| `buildscripts/`        | Script per build, linting, CI/CD                |

---

## I 3 concetti chiave da capire subito

### 1. ServiceContext, Client, OperationContext

Pensa a questi come alla gerarchia di contesti di Spring:

```
ServiceContext (1 per processo)          ≈ ApplicationContext
  └── Client (1 per connessione)         ≈ HttpSession
       └── OperationContext (1 per op)   ≈ HttpServletRequest
```

- **ServiceContext**: singleton globale. Contiene lo storage engine, il transport layer, la configurazione. Come `ApplicationContext` in Spring — accedi a tutto da qui.
- **Client**: rappresenta una connessione logica. Può eseguire un'operazione alla volta.
- **OperationContext** (`opCtx`): lo vedrai **ovunque**. È il contesto di ogni singola operazione — contiene deadline, cancellation token, stato della transazione. Viene passato come primo parametro a quasi tutte le funzioni.

### 2. Il sistema dei Command

In Spring hai `@RequestMapping`. Qui hai classi `Command` registrate globalmente:

```cpp
// Semplificato — in realtà è più complesso
class FindCmd : public Command {
    void run(OperationContext* opCtx, ...) {
        // parsing della query
        // creazione del piano di esecuzione
        // esecuzione e ritorno risultati
    }
};
```

Tutti i comandi sono registrati in un registry globale all'avvio — simile a come Spring scanna i controller.

### 3. Storage Engine come interfaccia

Come in JPA hai `EntityManager` che astrae il database sottostante, qui hai:

- **`KVEngine`**: interfaccia del motore key-value
- **`RecordStore`**: interfaccia per leggere/scrivere documenti in una collezione
- **`SortedDataInterface`**: interfaccia per gli indici
- **`RecoveryUnit`**: gestisce le transazioni e gli snapshot

WiredTiger implementa tutte queste interfacce. In teoria potresti sostituirlo con un altro engine (ed è successo — MongoDB ha supportato MMAPv1 in passato).

---

## Come metterci le mani: da dove partire

### Per capire il flusso di una query
1. Parti da `src/mongo/db/commands/query_cmd/` — il punto d'ingresso del comando `find`
2. Segui il flusso verso `src/mongo/db/query/` — dove avviene il parsing e l'ottimizzazione
3. Arriva a `src/mongo/db/exec/` — dove il piano viene eseguito

### Per capire le scritture
1. Parti da `src/mongo/db/commands/write_commands.cpp`
2. Segui verso `src/mongo/db/collection_crud/` — operazioni CRUD sulla collezione
3. Poi `src/mongo/db/storage/` — dove i dati vengono effettivamente scritti

### Per capire la replicazione
1. `src/mongo/db/repl/` — tutto il codice di replica set
2. `src/mongo/db/op_observer/` — come le scritture generano entry nell'oplog

### Per capire lo sharding
1. `src/mongo/s/` — il codice del router (`mongos`)
2. `src/mongo/db/s/` — il codice lato shard (`mongod` in modalità shard)

---

## Pattern ricorrenti nel codice

### Decorable (≈ Attributi sul contesto)
`ServiceContext`, `Client` e `OperationContext` sono "decorabili" — puoi attaccarci dati extra,
come `request.setAttribute()` in Java. Molti subsistemi registrano i propri dati sui contesti.

### OpObserver (≈ Event Listener / AOP)
Ogni volta che avviene una scrittura, una catena di `OpObserver` viene notificata.
È come avere `@EventListener` o `@Around` aspect su ogni operazione di modifica dati.

### Future/Promise (≈ CompletableFuture)
Il codice asincrono usa `Future<T>` e `Promise<T>`, molto simili a `CompletableFuture<T>` di Java 8+.
Supportano `.then()`, `.onError()`, `.onCompletion()` — stessa idea.

### WriteUnitOfWork (≈ @Transactional)
Racchiude operazioni in una transazione atomica sullo storage engine:
```cpp
WriteUnitOfWork wuow(opCtx);
// operazioni di scrittura...
wuow.commit();
// se non fai commit, il distruttore fa rollback (RAII)
```

---

## Il build system

MongoDB usa **Bazel** (non Maven/Gradle). I comandi principali:

```bash
# Compilare solo mongod (il server)
bazel build //src/mongo/db/mongod:install-mongod

# Compilare tutto (server + tools)
bazel build //src/mongo:install-dist

# Eseguire un test specifico
bazel test //src/mongo/db/query:query_test
```

I file `BUILD.bazel` sono l'equivalente dei `pom.xml` o `build.gradle` — definiscono target, dipendenze e regole di compilazione.

---

## Glossario rapido

| Termine MongoDB          | Significato                                              |
|--------------------------|----------------------------------------------------------|
| **BSON**                 | Binary JSON — formato di serializzazione dei documenti   |
| **Collection**           | Equivalente di una tabella SQL                           |
| **Document**             | Equivalente di una riga SQL (ma struttura flessibile)    |
| **OpLog**                | Log delle operazioni per la replicazione                 |
| **WiredTiger**           | Storage engine di default                                |
| **Replica Set**          | Gruppo di nodi che mantengono copie identiche dei dati   |
| **Shard**                | Un nodo che contiene un sottoinsieme dei dati            |
| **mongos**               | Router che smista le query agli shard giusti             |
| **mongod**               | Il processo server del database                          |
| **SBE**                  | Slot-Based Execution — motore di esecuzione query moderno|
| **CanonicalQuery**       | Forma normalizzata di una query dopo il parsing          |
| **PlanExecutor**         | L'oggetto che effettivamente esegue un piano di query    |
| **RecoveryUnit**         | Gestisce snapshot e transazioni nello storage engine     |
| **Baton**                | Meccanismo per yield/resume efficiente nelle operazioni  |

---

## Risorse interne al repo

Il repo contiene documentazione dettagliata per chi vuole approfondire:

- `docs/exception_architecture.md` — come funziona la gestione errori
- `docs/futures_and_promises.md` — programmazione asincrona
- `docs/command_dispatch.md` — pipeline di gestione richieste
- `docs/contexts.md` — ServiceContext/Client/OperationContext in dettaglio
- `docs/logging.md` — convenzioni di logging
- `docs/thread_pools.md` — infrastruttura di threading
- `docs/building.md` — istruzioni per la compilazione
- `src/mongo/transport/README.md` — layer di rete
- `src/mongo/executor/README.md` — esecuzione asincrona
- `src/mongo/db/query/README.md` — query engine (se presente)
