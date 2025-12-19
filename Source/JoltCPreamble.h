// Preamble: hand-written types

#ifdef _MSC_VER

#define JOLTC_VTABLE_HEADER
#define JOLTC_VTABLE_DESTRUCTOR void (*Destruct)(void *self);

#else

// Itanium C++ ABI (gcc, clang) puts additional information.
// offset_to_top is the number of bytes between the vtable pointer and the
// derived class.
// rtti is a pointer to runtime type information.
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

typedef struct JPH_Array {
    uint64_t mSize;
    uint64_t mCapacity;
    void *mElements;
} JPH_Array;

// StaticArray:
// struct {
//     uint64_t mSize
//     T mElements[N]
// }

typedef struct JPH_HashTable {
    void *mData;
    uint8_t *mControl;
    uint64_t mSize;
    uint64_t mMaxSize;
    uint64_t mLoadLeft;
} JPH_HashTable;

typedef JPH_HashTable JPH_UnorderedMap;
typedef JPH_HashTable JPH_UnorderedSet;

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
