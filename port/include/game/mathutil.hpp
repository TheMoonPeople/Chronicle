#pragma once

#include "common.h"

/**
 * @file
 * Declares the game's vector, geometry, interpolation, and trigonometry helpers.
 */

/**
 * How VectorInterpolate and AngleInterpolate move toward their target.
 */
// clang-format off
enum InterpolateMode {
    INTERPOLATE_STEP     = 0, /**< A fixed step, snapping when closer. */
    INTERPOLATE_FRACTION = 1, /**< The gap divided by the step. */
};

// clang-format on

/**
 * Runs a constructor over every element of an array.
 *
 * @mangled __construct_array
 * @address 0x1222D0
 * @size 0x12C
 */
/** Function used by the runtime to construct or destroy one object. */
typedef void (*MWRuntimeObjectFunction)(void *object, int mode);

extern "C" void __construct_array(void *array, MWRuntimeObjectFunction constructor, MWRuntimeObjectFunction destructor, unsigned int element_size, unsigned int count);

/**
 * Runs a constructor over every element of a newly allocated array.
 *
 * @mangled __construct_new_array
 * @address 0x122400
 * @size 0x14C
 */
extern "C" void *__construct_new_array(void *allocation, MWRuntimeObjectFunction constructor, MWRuntimeObjectFunction destructor, unsigned int element_size, unsigned int count);

/**
 * Frees storage that `operator new` handed out.
 *
 * @mangled __dl__FPv
 * @address 0x122550
 * @size 0x40
 */
void __dl(void *storage) throw();

/**
 * Reports whether a thrown type matches a catch clause's type.
 *
 * @mangled __throw_catch_compare
 * @address 0x122610
 * @size 0x26C
 */
extern "C" char __throw_catch_compare(const char *thrown_type, const char *caught_type, long *pointer_adjustment);

/**
 * Calls the handler for an exception a function did not declare.
 *
 * @mangled unexpected__3stdFv
 * @address 0x122880
 * @size 0x24
 */
extern "C" void unexpected__3stdFv();

/**
 * Calls the handler that ends the program after an unrecoverable exception.
 *
 * @mangled terminate__3stdFv
 * @address 0x1228B0
 * @size 0x24
 */
extern "C" void terminate__3stdFv();

/**
 * The default unexpected-exception handler, which terminates.
 *
 * @mangled duhandler__3stdFv
 * @address 0x1228E0
 * @size 0x24
 */
extern "C" void duhandler__3stdFv();

/**
 * The default terminate handler, which stops the program.
 *
 * @mangled dthandler__3stdFv
 * @address 0x122910
 * @size 0x1C
 */
extern "C" void dthandler__3stdFv();

/**
 * Links one object and its destructor into the runtime shutdown chain.
 */
struct MWGlobalDestructor {
    MWGlobalDestructor     *next;       /**< Next object destroyed during shutdown. */
    MWRuntimeObjectFunction destructor; /**< Function that destroys the registered object. */
    void                   *object;     /**< Object passed to the destructor. */
};

/**
 * Handler that std::terminate calls to end the program.
 */
extern "C" void (*thandler__3std)() __attribute__((section(".data"))) __attribute__((aligned(8)));

/**
 * Handler that std::unexpected calls for an exception a function did not declare.
 */
extern "C" void (*uhandler__3std)() __attribute__((section(".data"))) __attribute__((aligned(8)));

/**
 * Most recently registered global object, heading the list destroyed at exit.
 */
extern "C" MWGlobalDestructor *__global_destructor_chain __attribute__((section(".bss")));

/**
 * Records one global object so that its destructor runs at exit.
 *
 * @mangled __register_global_object
 * @address 0x122930
 * @size 0x24
 */
extern "C" void *__register_global_object(void *object, MWRuntimeObjectFunction destructor, MWGlobalDestructor *record);

/**
 * Calls each static initializer in the table from the first pointer up to the second.
 *
 * @mangled __initialize_cpp_rts
 * @address 0x122960
 * @size 0x54
 */
extern "C" void __initialize_cpp_rts(void *first, void *last, void *overlay_start, void *overlay_end);

/**
 * Reads an unsigned number out of a mangled type name.
 *
 * @mangled __DecodeUnsignedNumber__FPcPUi
 * @address 0x1229C0
 * @size 0xA0
 */
char *__DecodeUnsignedNumber(char *encoded, unsigned int *value);

/**
 * Reads a signed number out of a mangled type name.
 *
 * @mangled __DecodeSignedNumber__FPcPi
 * @address 0x122A60
 * @size 0xA0
 */
char *__DecodeSignedNumber(char *encoded, int *value);

/**
 * Ends a catch clause and releases the exception it caught.
 *
 * @mangled __end__catch
 * @address 0x122B00
 * @size 0x38
 */
/**
 * Holds the object and cleanup routine associated with one active catch clause.
 */
struct MWCatchRecord {
    void                   *object;       /**< Exception object owned by the catch clause. */
    const char             *type_info;    /**< Encoded type name of the exception object. */
    MWRuntimeObjectFunction destructor;   /**< Routine that destroys the exception object. */
    void                   *sub_object;   /**< Base-class subobject the catch clause receives. */
    int                     pointer_copy; /**< Copy of a thrown pointer value. */
    void                   *stack_top;    /**< Stack pointer, or the throwing function's exception specification. */
};

/**
 * The decoded exception specification of a function that let an exception escape.
 */
struct MWExceptionSpecification {
    unsigned int   count;           /**< Number of type names the specification allows. */
    unsigned int   unused_unsigned; /**< Unsigned number encoded after the count; decoded past and not read. */
    int            unused_signed;   /**< Signed number encoded after it; decoded past and not read. */
    unsigned char *types;           /**< Unaligned little-endian pointers to the allowed type names. */
};

extern "C" void __end__catch(MWCatchRecord *record);

/**
 * Raises an exception a function did not declare, through the unexpected handler.
 *
 * @mangled __unexpected
 * @address 0x122B40
 * @size 0x1C0
 */
extern "C" void __unexpected(void *exception_record);

/**
 * Starts the MetroWerks runtime.
 *
 * @mangled mwInit
 * @address 0x122DA0
 * @size 0x40
 */
extern "C" void mwInit(int argc, const char **argv, const char **envp);

/**
 * The header at the start of an overlay image read off the disc.
 */
struct OverlayHeader {
    u8    unk_00[0x14];
    int   bss_size;        /**< Bytes of zeroed storage that follow the loaded image. */
    void *static_init;     /**< First entry of the overlay's static initializer table. */
    void *static_init_end; /**< End of the overlay's static initializer table. */
    u8    unk_20[0x20];
};

STATIC_ASSERT(sizeof(OverlayHeader) == 0x40);

/**
 * Starts the overlay loader and records where overlays are read to.
 *
 * @mangled mwOverlayInit
 * @address 0x122DE0
 * @size 0x8C
 */
extern "C" void mwOverlayInit(void *overlay, int size);

/**
 * Tells the runtime that an overlay has finished loading.
 *
 * @mangled MWNotifyOverlayLoaded
 * @address 0x122E70
 * @size 0x8
 */
extern "C" void MWNotifyOverlayLoaded(void *address);

/**
 * Reads one overlay image off the disc.
 *
 * @mangled mwBload
 * @address 0x122E80
 * @size 0xB4
 */
extern "C" int mwBload(char *path, void *buffer);

/**
 * Reads an overlay into memory and gives back whether it succeeded.
 *
 * @mangled mwLoadOverlay
 * @address 0x122F40
 * @size 0x70
 */
extern "C" int mwLoadOverlay(char *path, void *address);

/**
 * @mangled VectorMax__FPfPfPf
 * @address 0x122FB0
 * @size 0x18
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b);

/**
 * @mangled VectorMax__FPfPfPfPf
 * @address 0x122FD0
 * @size 0x20
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b, float *c);

/**
 * @mangled VectorMax__FPfPfPfPfPf
 * @address 0x122FF0
 * @size 0x28
 * @unknownret
 */
void VectorMax(float *max, float *a, float *b, float *c, float *d);

/**
 * @mangled VectorMin__FPfPfPf
 * @address 0x123020
 * @size 0x18
 * @unknownret
 */
void VectorMin(float *min, float *a, float *b);

/**
 * @mangled VectorMin__FPfPfPfPfPf
 * @address 0x123040
 * @size 0x28
 * @unknownret
 */
void VectorMin(float *min, float *a, float *b, float *c, float *d);

/**
 * @mangled VectorMaxMin__FPfPfPfPf
 * @address 0x123070
 * @size 0x20
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b);

/**
 * @mangled VectorMaxMin__FPfPfPfPfPf
 * @address 0x123090
 * @size 0x2C
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c);

/**
 * @mangled VectorMaxMin__FPfPfPfPfPfPf
 * @address 0x1230C0
 * @size 0x38
 * @unknownret
 */
void VectorMaxMin(float *max, float *min, float *a, float *b, float *c, float *d);

/**
 * @mangled PlaneNormal__FPfPfPfPf
 * @address 0x123100
 * @size 0x28
 * @unknownret
 */
void PlaneNormal(float *normal, float *v0, float *v1, float *v2);

/**
 * @mangled DistPlanePoint__FPfPfPf
 * @address 0x123130
 * @size 0x4C
 */
float DistPlanePoint(float *normal, float *on_plane, float *point);

/**
 * @mangled ReflectionPlane__FPfPfPfPf
 * @address 0x123180
 * @size 0xA8
 */
float ReflectionPlane(float *normal, float *on_plane, float *point, float *reflection);

/**
 * @mangled IntersectionPoint_line_poly3__FPfPfPfPfPfPfPf
 * @address 0x123230
 * @size 0x144
 */
int IntersectionPoint_line_poly3(float *from, float *to, float *v0, float *v1, float *v2, float *normal, float *hit);

/**
 * @mangled Check_Point_Poly3_XYZ__FPfPfPfPfPf
 * @address 0x123380
 * @size 0x1DC
 */
int Check_Point_Poly3_XYZ(float *point, float *v0, float *v1, float *v2, float *normal);

/**
 * @mangled DistVector__FPf
 * @address 0x123560
 * @size 0x30
 */
float DistVector(float *vector);

/**
 * Returns the distance between two positions.
 *
 * @mangled DistVector__FPfPf
 * @address 0x123590
 * @size 0x38
 */
float DistVector(float *a, float *b);

/**
 * @mangled MulMatrix__FPA4_fPA4_fPA4_f
 * @address 0x1235D0
 * @size 0x78
 * @unknownret
 */
void MulMatrix(float (*product)[4], float (*left_matrix)[4], float (*right_matrix)[4]);

/**
 * @mangled RotMatrixY__FPA4_ff
 * @address 0x123650
 * @size 0x74
 * @unknownret
 */
void RotMatrixY(float (*matrix)[4], float angle_y);

/**
 * @mangled LookAtMatrixZ__FPA4_fPf
 * @address 0x1236D0
 * @size 0x10C
 * @unknownret
 */
void LookAtMatrixZ(float (*matrix)[4], float *direction);

/**
 * @mangled ApplyMatrixN__FPA4_fPA4_fPA4_fi
 * @address 0x1237E0
 * @size 0x60
 * @unknownret
 */
void ApplyMatrixN(float (*out)[4], float (*matrix)[4], float (*in)[4], int count);

/**
 * @mangled VectorInterpolate__FPfPfPffi
 * @address 0x123840
 * @size 0x18C
 * @unknownret
 */
void VectorInterpolate(float *out, float *from, float *to, float step, int mode);

/**
 * @mangled AngleInterpolate__Ffffi
 * @address 0x1239D0
 * @size 0x160
 */
float AngleInterpolate(float from, float to, float step, int mode);

/**
 * @mangled AngleCmp__Ffff
 * @address 0x123B30
 * @size 0xA8
 * @unknownret
 */
int AngleCmp(float a, float b, float tolerance);

/**
 * Wraps one angle into the half turn either side of zero.
 *
 * @mangled AngleLimit__Ff
 * @address 0x123BE0
 * @size 0xD0
 */
float AngleLimit(float angle);

/**
 * @mangled rnd__Fv
 * @address 0x123CB0
 * @size 0x3C
 */
float rnd();

/**
 * @mangled nrnd__Fv
 * @address 0x123CF0
 * @size 0xC0
 */
float nrnd();

/**
 * @mangled CreateSinTable__Fv
 * @address 0x123DB0
 * @size 0x90
 * @unknownret
 */
void CreateSinTable();

/**
 * @mangled Sinf__Ff
 * @address 0x123E40
 * @size 0xB8
 */
float Sinf(float angle);

/**
 * @mangled Cosf__Ff
 * @address 0x123F00
 * @size 0x28
 */
float Cosf(float angle);

/** + 4 more not-yet-named function(s) in this range (IDA/disassembler could not name them) */

/**
 * First entry of the table of static initialisers the runtime calls at startup; the linker script places it.
 */
extern void (*__static_init[])();

/**
 * End of the table of static initialisers the runtime calls at startup; the linker script places it.
 */
extern void (*__static_init_end[])();

/**
 * Destructor of std::exception.
 *
 * @mangled __dt__Q23std9exceptionFv
 * @address 0x122590
 * @size 0x6c
 */
extern "C" void *__dt__Q23std9exceptionFv(void **self, short flag) throw();

/**
 * std::exception::what.
 *
 * @mangled what__Q23std9exceptionCFv
 * @address 0x122600
 * @size 0xc
 */
extern "C" const char *what__Q23std9exceptionCFv(const void *exception);

/**
 * Destructor of std::bad_exception.
 *
 * @mangled __dt__Q23std13bad_exceptionFv
 * @address 0x122D00
 * @size 0x84
 */
extern "C" void *__dt__Q23std13bad_exceptionFv(void **self, short flag) throw();

/**
 * std::bad_exception::what.
 *
 * @mangled what__Q23std13bad_exceptionCFv
 * @address 0x122D90
 * @size 0xc
 */
extern "C" const char *what__Q23std13bad_exceptionCFv(const void *exception);

/**
 * Run-time type information record of std::exception.
 */
extern const unsigned char __RTTI__Q23std9exception[];

struct MWBaseClass;

/**
 * The run-time type information record of a class: its name and its bases.
 */
struct MWTypeInfo {
    const char        *name;  /**< The class's qualified name. */
    const MWBaseClass *bases; /**< Its base classes, ended by a null entry, or null. */
};

/**
 * One base class in a class's run-time type information.
 */
struct MWBaseClass {
    const MWTypeInfo *type;   /**< The base class's record, or null to end the list. */
    int               offset; /**< Where the base sits inside the derived object. */
};

struct MWSingleBaseClass {
    MWBaseClass       base;
    const MWTypeInfo *end;
};

/**
 * Qualified name of std::bad_exception.
 */
extern const char BadExceptionTypeName[];

/**
 * Qualified name of std::exception.
 */
extern const char ExceptionTypeName[];

/**
 * The runtime's own run-time type information record of std::exception.
 */
extern const MWTypeInfo ExceptionTypeInfo;

/**
 * The base classes of std::bad_exception: std::exception alone.
 */
extern const MWSingleBaseClass BadExceptionBaseClasses;

/**
 * Run-time type information record of std::bad_exception.
 */
extern const MWTypeInfo __RTTI__Q23std13bad_exception;

/**
 * What std::exception::what returns.
 */
extern const char ExceptionWhat[];

/**
 * What std::bad_exception::what returns.
 */
extern const char BadExceptionWhat[];
