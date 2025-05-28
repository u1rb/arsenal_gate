Below are a handful of strictly‑C techniques that keep an abstraction layer but try to get rid of as much indirection cost as possible.  
All examples compile as C99 or later; the ideas also work in C89 with minor edits.

---

## 1. Hoist the function pointer out of the hot loop

If you **must** keep a function pointer, load it once, not every trip through the loop.

```c
uint64_t run_stream(PacketStream *s, size_t n_pkts)
{
    /* copy the pointer to a local register‑resident variable */
    uint64_t (*next_fn)(PacketStream *) = s->next;

    uint64_t sum = 0;
    for(size_t i = 0; i < n_pkts; ++i)
        sum += next_fn(s);        /* no extra indirection inside loop */
    return sum;
}
```

Cost removed  
• one extra load of `s->next` per packet.

---

## 2. Embed the concrete state inside the generic object

Instead of

```c
struct PacketStream {
    uint64_t (*next)(struct PacketStream *);
    void     *private_data;           /* heap allocated */
};
```

put the implementation directly behind the facade:

```c
struct PS_NoMerge {
    PacketStream base;  /* first: we can up‑cast to PacketStream* */
    NoMerger      impl; /* concrete state – on the same cache line */
};

static uint64_t no_merge_next_embedded(PacketStream *ps)
{
    struct PS_NoMerge *self = (struct PS_NoMerge *)ps;
    return no_merger_next(&self->impl);   /* single cast, no extra ptr */
}

void ps_nomerge_init(struct PS_NoMerge *obj, /* … */)
{
    obj->base.next = no_merge_next_embedded;
    no_merger_init(&obj->impl /* … */);
}
```

Benefits  
• no heap traffic, one pointer less to chase, better cache locality.  

---

## 3. Select the concrete function **once** with a `switch`

If you know the kind of stream up‑front, pay the indirection outside the hot path:

```c
typedef enum { PS_NOMERGE, PS_HEAP, PS_LOSERTREE } PsKind;

uint64_t run(PacketStream *ps, PsKind k, size_t n)
{
    switch(k) {        /* executed once */
    case PS_NOMERGE:     return run_nm((NoMerger*)ps->private_data, n);
    case PS_HEAP:        return run_hm((HeapMerger*)ps->private_data, n);
    case PS_LOSERTREE:   return run_lt((LoserTree*)ps->private_data, n);
    }
    return 0;
}
```

Inside `run_nm`, `run_hm`, … only direct calls remain; the compiler is free to inline them.

---

## 4. Use `_Generic` for compile‑time dispatch

Since C11 you can do a tiny bit of type‑based polymorphism without any runtime cost:

```c
#define stream_next(s)                           \
    _Generic((s),                                \
             NoMerger *:      no_merger_next,    \
             HeapMerger *:    heap_next,         \
             LoserTree *:     loser_next         \
    )(s)                                         /* direct call */

uint64_t sum_packets(NoMerger *nm, size_t n)
{
    uint64_t sum = 0;
    for(size_t i = 0; i < n; ++i)
        sum += stream_next(nm);     /* expands to no_merger_next(nm); */
    return sum;
}
```

Cost removed  
• the function pointer indirection is eliminated completely; the call can inline.

---

## 5. Generate the boiler‑plate with macros

If you only want “polymorphism” for a handful of stream types, let the pre‑processor write the boring code at build time:

```c
#define DECL_STREAM(T, prefix)                                   \
    static inline uint64_t prefix##_loop(T *obj, size_t n) {     \
        uint64_t sum = 0;                                        \
        for(size_t i = 0; i < n; ++i)                            \
            sum += prefix##_next(obj);                           \
        return sum;                                              \
    }

DECL_STREAM(NoMerger, no_merger)
DECL_STREAM(HeapMerger, heap)
```

Each translation unit gets its own specialised, fully inlinable version; zero dispatch overhead.

---

## 6. Pool or stack‑allocate “opaque” memory

If you still want “one pointer for private data” but hate `malloc`, carve out space up‑front:

```c
typedef union {
    NoMerger   nm;
    HeapMerger hm;
    LoserTree  lt;
} MaxStreamState;          /* large enough for any helper */

struct PacketStream {
    uint64_t (*next)(struct PacketStream *);
    MaxStreamState st;     /* storage lives _here_ */
};

static uint64_t nm_next_wrapped(PacketStream *ps)
{
    return no_merger_next(&ps->st.nm);   /* no heap, no cast */
}
```

---

# Take‑away checklist

1. Load (function) pointers outside hot loops.  
2. Keep concrete state contiguous with the facade (`struct` embedding, unions).  
3. Do one‑time selection via `switch`, table lookup, or `_Generic`.  
4. Let the pre‑processor generate specialised versions when performance matters.  
5. Favour stack / arena / pool allocation over `malloc` for per‑stream state.  

All of the above keep the *logical* abstraction while removing nearly all of the per‑packet indirection cost that hurt the single‑stream benchmark.