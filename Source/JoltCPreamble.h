// Preamble: hand-written types

#ifdef _MSC_VER

#define JOLTC_VTABLE_HEADER
#define JOLTC_VTABLE_DESTRUCTOR void (*Destruct)(void *self);

#else

// Itanium C++ ABI (gcc, clang) puts additional information.
// offset_to_top is the number of bytes between the vtable pointer and the
// derived class.
// rtti is a pointer to runtime type information. Jolt is compiled using
// -fno-rtti by default, but it does not change the layout of this struct,
// only that this pointer is NULL (otherwise ABI compatibility would be broken).
// Delete and Destruct are two versions of the destructors called either when
// calling delete or when the object goes out of scope (on the stack). They
// are always present even if the class does not have a virtual destructor.
#define JOLTC_VTABLE_HEADER \
    uint64_t offset_to_top; \
    void *rtti; \
    void (*Delete)(void *self); \
    void (*Destruct)(void *self);

#define JOLTC_VTABLE_DESTRUCTOR

#endif

typedef uint16_t JPH_ObjectLayer;
typedef uint8_t JPH_BroadPhaseLayer;

typedef struct JPH_StridedPtr {
    uint8_t *mPtr;
    int mStride;
} JPH_StridedPtr;

#define JPH_StaticArrayT(T, N) struct { \
    uint32_t mSize; \
    T mElements[N]; \
}

typedef struct JPH_Array {
    uint64_t mSize;
    uint64_t mCapacity;
    void *mElements;
} JPH_Array;

typedef struct JPH_HashTable {
    void *mData;
    uint8_t *mControl;
    uint64_t mSize;
    uint64_t mMaxSize;
    uint64_t mLoadLeft;
} JPH_HashTable;

typedef JPH_HashTable JPH_UnorderedMap;
typedef JPH_HashTable JPH_UnorderedSet;

struct JPH_LFHMAllocator;

typedef struct JPH_LockFreeHashMap {
    struct JPH_LFHMAllocator *mAllocator;
#ifdef JPH_ENABLE_ASSERTS
    uint32_t mNumKeyValues;
#endif
    uint32_t *mBuckets;
    uint32_t mNumBuckets;
    uint32_t mMaxBuckets;
} JPH_LockFreeHashMap;

#define JPH_LockFreeHashMap_KeyValueT(Key, Value) struct { \
    Key mKey; \
    uint32_t mNextOffset; \
    Value mValue; \
}

typedef struct JPH_RefTarget {
    uint32_t mRefCount;
} JPH_RefTarget;

#define JOLTC_DECLARE_REFTARGET_FUNCTIONS(T) \
    void T ## _SetEmbedded(T *self); \
    uint32_t T ## _GetRefCount(const T *self); \
    void T ## _AddRef(T *self); \
    void T ## _Release(T *self);

#define JOLTC_IMPL_REFTARGET_FUNCTIONS(T) \
    void T ## _SetEmbedded(T *self) { ToCpp(self)->SetEmbedded(); } \
    uint32_t T ## _GetRefCount(const T *self) { return ToCpp(self)->GetRefCount(); } \
    void T ## _AddRef(T *self) { ToCpp(self)->AddRef(); } \
    void T ## _Release(T *self) { ToCpp(self)->Release(); }

typedef struct JPH_Vector2 {
    float mF32[2];
} JPH_Vector2;

typedef struct JPH_Vector3 {
    float mF32[3];
} JPH_Vector3;

typedef struct JPH_Matrix22 {
    JPH_Vector2 mCol[2];
} JPH_Matrix22;

typedef struct JPH_Matrix33 {
    JPH_Vector3 mCol[3];
} JPH_Matrix33;

struct JPH_TransformedShape;

typedef struct JPH_CollisionCollector {
    float mEarlyOutFraction;
    struct JPH_TransformedShape *mContext;
} JPH_CollisionCollector;

// From libstdc++-v3 (GCC standard library):

typedef struct JPH_StringView {
    size_t length;
    const char *str;
} JPH_StringView;

typedef struct JPH_String {
    char *ptr;
    size_t length;
    union {
        char local_buffer[16];
        size_t capacity;
    };
} JPH_String;

typedef struct JPH_MutexBase {
    pthread_mutex_t mutex;
} JPH_MutexBase;

typedef struct JPH_SharedMutexBase {
    pthread_rwlock_t rw_lock;
} JPH_SharedMutexBase;

typedef struct JPH_ThreadId {
    pthread_t handle;
} JPH_ThreadId;

typedef uint8_t JPH_Result_EState;
enum {
    JPH_Result_EState_Invalid = 0,
    JPH_Result_EState_Valid = 1,
    JPH_Result_EState_Error = 2,
};

#define JPH_ResultT(T) struct { \
    union { \
        T mResult; \
        JPH_String mError; \
    }; \
    JPH_Result_EState mState; \
}

typedef struct alignas(16) JPH_Function {
    uint8_t opaque[32];
} JPH_Function;
