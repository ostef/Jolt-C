# Notes on implementing a C interface to C++ code
It might not be possible/practical to generate everything as a one-to-one representation of what Jolt provides, for various reasons such as ABI awkwardness, usage of templates, usage of abstract classes/interfaces etc.

## Interfaces/abstract classes:
We want to provide a nice way to use interfaces from Jolt, ideally providing a way for users to implement those interfaces from C land. Here is one way we could do it:

C++:
```c++
class IFoo {
public:
    virtual ~IFoo() = 0;

    virtual int GetValue() const = 0;
    virtual void SetValue(int value) = 0;
}
```

C header:
```c
// Simply an interface to the functions, not a 1-to-1 ABI compliant struct
typedef struct IFoo_VTable {
    void Destructor(void *self);
    int GetValue(const void *self);
    void SetValue(void *self, int value);
} IFoo_VTable;

typedef struct IFoo {
    void *self;
    IFoo_VTable vtable; // Not a pointer for faster access
} IFoo;

void DoSomethingWithIFoo(IFoo foo);
```
C implementation:
```c++
final struct IFoo_Adapter : JPH::IFoo {
    IFoo base;

    explicit IFoo_Adapter(IFoo foo) {
        base = foo;
    }

    virtual ~IFoo_Adapter() override {
        base.vtable.Destructor(base.self);
    }

    virtual int GetValue() const override {
        return base.vtable.GetValue(base.self);
    }

    virtual void SetValue(int value) override {
        base.vtable.SetValue(base.self, value);
    }
}

void DoSomethingWithIFoo(IFoo foo) {
    JPH::DoSomethingWithIFoo(IFoo_Adapter(foo));
}
```
To implement and use the interface from C land:
```c
typedef struct MyFoo {
    int value;
} MyFoo;

void MyFoo_Destructor(void *self) {
    printf("MyFoo destroyed\n");
}

int MyFoo_GetValue(const void *self) {
    return ((MyFoo *)self)->value;
}

void MyFoo_SetValue(void *self, int value) {
    ((MyFoo *)self)->value = value;
}

const IFoo_VTable MyFoo_VTable = {
    .Destructor=MyFoo_Destructor,
    .GetValue=MyFoo_GetValue,
    .SetValue=MyFoo_SetValue,
};

IFoo MyFooToIFoo(MyFoo *foo) {
    return {.self=foo, .vtable=MyFoo_VTable};
}

int main() {
    MyFoo my_foo = {};
    DoSomethingWithIFoo(MyFooToIFoo(&my_foo));
}
```
The advantage with using this method is that we do not have to respect the ABI of the C++ compiler that was used to compile Jolt, while still allowing us to implement the interface from C land. Another big advantage is that it is straightforward to implement and can be generated automatically. One disadvantage of this method is its verbosity.

## Templates
A very important consideration when making C bindings for Jolt is the heavy usage of templates. I am not sure what the strategy should be for templates; I think we need a per case solution. A fallback to the brute force approach could work for certain templates: gather all template instantiations and generate code for each of them. The thing to consider with this approach is how we handle template arguments of non obvious types (e.g. **Array<StridedPtr<RefTarget<Body> > >**)

Here are all the templates that we would need to support:
* **JPH::Array**: the nice thing about this template is that T is used by pointer, so we could provide a non specialized JPH_Array struct. Furthermore, the default STLAllocator does not contain any fields and is just a simple static interface to external allocator functions, so we don't have to worry about it (unless another allocator is used sometimes)
* **JPH::Vector**, **JPH::Matrix**: only a few instantiation of Vector<2>, Vector<3> and Matrix<2,2>, Matrix<3,3> are used, so this one is easy.
* **JPH::Result**: not that hard to support with macros/codegen since I don't think we need to implement any function for it.
* **JPH::StaticArray**: same as result, and the functions are optional because operations are easy to write inline (e.g. `array.data[array.count++] = value`)
* **JPH::StridedPtr**: the layout does not change and the underlying type is always *uint8_t \**
* **JPH::HashTable**: same as **JPH::Array**, can be implemented using a void pointer, and we don't provide functions to interact with the table or we provide them on a per case basis (e.g. type **Foo** has a hash table member named **myTable**, so we would provide a few **Foo_myTable_XXX** functions if needed).
* **JPH::RayCastT**, **JPH::ShapeCastT**: could handwrite the few instantiations that are actually used, but using macros would probably be better because I think we would need to implement a few functions.

## std types
A big PITA is that Jolt uses a lot of the types from the C++ standard library such as **std::mutex**, **std::basic_string**, **std::basic_istringstream**, **std::atomic**. I don't know what to do with these especially considering their implementation is compiler/compiler version/platform dependent. Fuck C++. One option is to provide opaque types and provide functions to access the members of these types. However this is verbose and very inconvenient. Another option is to wrap only the std type around an opaque structure that has the same size and alignment requirements. Last option that I can think of is to modify Jolt to use our own implementation of these when it matters.

* **std::atomic**: probably mostly a wrapper around the value type with specific alignment requirements, so maybe this is not so hard?

## Function overloads
I don't know if function overloading is heavily used in the APIs we want to expose, but if that is the case we want to provide good names for each overload.
