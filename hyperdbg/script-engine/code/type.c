/**
 * @file type.c
 * @author M.H. Gholamrezaei (mh@hyperdbg.org)
 *
 * @brief Routines for handling variable types
 * @details
 * @version 0.1
 * @date 2020-10-22
 *
 * @copyright This project is released under the GNU Public License v3.
 *
 */
#include "pch.h"

typedef struct _TYPE_ALLOCATION_NODE
{
    PVARIABLE_TYPE                Type;
    struct _TYPE_ALLOCATION_NODE * Next;
} TYPE_ALLOCATION_NODE, *PTYPE_ALLOCATION_NODE;

typedef struct _STRUCT_TAG_NODE
{
    char *                    Name;
    PVARIABLE_TYPE            Type;
    struct _STRUCT_TAG_NODE * Next;
} STRUCT_TAG_NODE, *PSTRUCT_TAG_NODE;

typedef struct _TYPEDEF_NODE
{
    char *                Name;
    PVARIABLE_TYPE        Type;
    struct _TYPEDEF_NODE * Next;
} TYPEDEF_NODE, *PTYPEDEF_NODE;

static PTYPE_ALLOCATION_NODE TypeAllocations;
static PSTRUCT_TAG_NODE      StructTags;
static PTYPEDEF_NODE         Typedefs;

static PVARIABLE_TYPE
AllocateType(VOID)
{
    PVARIABLE_TYPE Type = (PVARIABLE_TYPE)calloc(1, sizeof(VARIABLE_TYPE));
    PTYPE_ALLOCATION_NODE Node;

    if (!Type)
    {
        return NULL;
    }

    Node = (PTYPE_ALLOCATION_NODE)calloc(1, sizeof(TYPE_ALLOCATION_NODE));
    if (!Node)
    {
        free(Type);
        return NULL;
    }

    Node->Type      = Type;
    Node->Next      = TypeAllocations;
    TypeAllocations = Node;
    return Type;
}

static unsigned int
AlignTo(unsigned int Value, unsigned int Alignment)
{
    return (Value + Alignment - 1) & ~(Alignment - 1);
}

VOID
InitializeTypeContext(VOID)
{
    TypeAllocations = NULL;
    StructTags      = NULL;
    Typedefs        = NULL;
}

VOID
UninitializeTypeContext(VOID)
{
    while (Typedefs)
    {
        PTYPEDEF_NODE Next = Typedefs->Next;
        free(Typedefs->Name);
        free(Typedefs);
        Typedefs = Next;
    }

    while (StructTags)
    {
        PSTRUCT_TAG_NODE Next = StructTags->Next;
        free(StructTags->Name);
        free(StructTags);
        StructTags = Next;
    }

    while (TypeAllocations)
    {
        PTYPE_ALLOCATION_NODE Next = TypeAllocations->Next;
        PSTRUCT_MEMBER Member = TypeAllocations->Type->Members;
        while (Member)
        {
            PSTRUCT_MEMBER NextMember = Member->Next;
            free(Member->Name);
            free(Member);
            Member = NextMember;
        }
        free(TypeAllocations->Type->TagName);
        free(TypeAllocations->Type);
        free(TypeAllocations);
        TypeAllocations = Next;
    }
}

PVARIABLE_TYPE
FindStructType(const char * TagName)
{
    PSTRUCT_TAG_NODE Node;
    for (Node = StructTags; Node; Node = Node->Next)
    {
        if (!strcmp(Node->Name, TagName))
        {
            return Node->Type;
        }
    }
    return NULL;
}

PVARIABLE_TYPE
DeclareStructType(const char * TagName)
{
    PVARIABLE_TYPE Existing = FindStructType(TagName);
    PSTRUCT_TAG_NODE Node;
    PVARIABLE_TYPE Type;

    if (Existing)
    {
        return Existing;
    }

    Type = AllocateType();
    Node = (PSTRUCT_TAG_NODE)calloc(1, sizeof(STRUCT_TAG_NODE));
    if (!Type || !Node)
    {
        free(Node);
        return NULL;
    }

    Type->Kind    = TY_STRUCT;
    Type->TagName = PlatformStrDup(TagName);
    Node->Name    = PlatformStrDup(TagName);
    Node->Type    = Type;
    Node->Next    = StructTags;
    StructTags    = Node;
    return Type;
}

BOOLEAN
AddStructMember(PVARIABLE_TYPE StructType, const char * Name, PVARIABLE_TYPE MemberType)
{
    PSTRUCT_MEMBER Member;
    PSTRUCT_MEMBER * Tail;
    unsigned int Order = 0;

    if (!StructType || StructType->Kind != TY_STRUCT || StructType->IsComplete)
    {
        return FALSE;
    }

    Tail = &StructType->Members;
    while (*Tail)
    {
        if (!strcmp((*Tail)->Name, Name))
        {
            return FALSE;
        }
        Order++;
        Tail = &(*Tail)->Next;
    }

    Member = (PSTRUCT_MEMBER)calloc(1, sizeof(STRUCT_MEMBER));
    if (!Member)
    {
        return FALSE;
    }
    Member->Name             = PlatformStrDup(Name);
    Member->Type             = MemberType;
    Member->DeclarationOrder = Order;
    *Tail                    = Member;
    return TRUE;
}

PSTRUCT_MEMBER
FindStructMember(PVARIABLE_TYPE StructType, const char * Name)
{
    PSTRUCT_MEMBER Member;

    if (!StructType || StructType->Kind != TY_STRUCT)
    {
        return NULL;
    }

    for (Member = StructType->Members; Member; Member = Member->Next)
    {
        if (!strcmp(Member->Name, Name))
        {
            return Member;
        }
    }

    return NULL;
}

BOOLEAN
CompleteStructType(PVARIABLE_TYPE StructType)
{
    unsigned int Offset = 0;
    unsigned int Alignment = 1;
    PSTRUCT_MEMBER Member;

    if (!StructType || StructType->Kind != TY_STRUCT || StructType->IsComplete)
    {
        return FALSE;
    }

    for (Member = StructType->Members; Member; Member = Member->Next)
    {
        if (!Member->Type || !Member->Type->Align ||
            (Member->Type->Kind == TY_STRUCT && !Member->Type->IsComplete))
        {
            return FALSE;
        }
        if (Offset > 0x7fffffffU - ((unsigned int)Member->Type->Align - 1))
        {
            return FALSE;
        }
        Offset         = AlignTo(Offset, (unsigned int)Member->Type->Align);
        Member->Offset = Offset;
        if ((unsigned int)Member->Type->Size > 0x7fffffffU - Offset)
        {
            return FALSE;
        }
        Offset += (unsigned int)Member->Type->Size;
        if ((unsigned int)Member->Type->Align > Alignment)
        {
            Alignment = (unsigned int)Member->Type->Align;
        }
    }

    if (StructType->Members && Offset > 0x7fffffffU - (Alignment - 1))
    {
        return FALSE;
    }
    StructType->Align      = (int)Alignment;
    StructType->Size       = StructType->Members ? (int)AlignTo(Offset, Alignment) : 1;
    StructType->IsComplete = TRUE;
    return TRUE;
}

PVARIABLE_TYPE
CreatePointerType(PVARIABLE_TYPE BaseType)
{
    PVARIABLE_TYPE Type = AllocateType();
    if (Type)
    {
        Type->Kind       = TY_PTR;
        Type->Size       = 8;
        Type->Align      = 8;
        Type->IsUnsigned = TRUE;
        Type->Base       = BaseType;
        Type->IsComplete = TRUE;
    }
    return Type;
}

PVARIABLE_TYPE
CreateArrayType(PVARIABLE_TYPE BaseType, unsigned int ArrayLength)
{
    PVARIABLE_TYPE Type;
    if (!BaseType || !ArrayLength ||
        (BaseType->Kind == TY_STRUCT && !BaseType->IsComplete) ||
        (unsigned int)BaseType->Size > 0x7fffffffU / ArrayLength)
    {
        return NULL;
    }

    Type = AllocateType();
    if (Type)
    {
        Type->Kind       = TY_ARRAY;
        Type->Size       = BaseType->Size * (int)ArrayLength;
        Type->Align      = BaseType->Align;
        Type->Base       = BaseType;
        Type->ArrayLen   = (int)ArrayLength;
        Type->IsComplete = TRUE;
    }
    return Type;
}

BOOLEAN
AddTypedefType(const char * Name, PVARIABLE_TYPE Type)
{
    PTYPEDEF_NODE Node;
    for (Node = Typedefs; Node; Node = Node->Next)
    {
        if (!strcmp(Node->Name, Name))
        {
            return FALSE;
        }
    }

    Node = (PTYPEDEF_NODE)calloc(1, sizeof(TYPEDEF_NODE));
    if (!Node)
    {
        return FALSE;
    }
    Node->Name = PlatformStrDup(Name);
    Node->Type = Type;
    Node->Next = Typedefs;
    Typedefs   = Node;
    return TRUE;
}

VARIABLE_TYPE * VARIABLE_TYPE_UNKNOWN = &(VARIABLE_TYPE) {TY_UNKNOWN};

VARIABLE_TYPE * VARIABLE_TYPE_VOID = &(VARIABLE_TYPE) {TY_VOID, 1, 1};
VARIABLE_TYPE * VARIABLE_TYPE_BOOL = &(VARIABLE_TYPE) {TY_BOOL, 1, 1};

VARIABLE_TYPE * VARIABLE_TYPE_CHAR  = &(VARIABLE_TYPE) {TY_CHAR, 1, 1};
VARIABLE_TYPE * VARIABLE_TYPE_SHORT = &(VARIABLE_TYPE) {TY_SHORT, 2, 2};
VARIABLE_TYPE * VARIABLE_TYPE_INT   = &(VARIABLE_TYPE) {TY_INT, 4, 4};
VARIABLE_TYPE * VARIABLE_TYPE_LONG  = &(VARIABLE_TYPE) {TY_LONG, 8, 8};

VARIABLE_TYPE * VARIABLE_TYPE_UCHAR  = &(VARIABLE_TYPE) {TY_CHAR, 1, 1, TRUE};
VARIABLE_TYPE * VARIABLE_TYPE_USHORT = &(VARIABLE_TYPE) {TY_SHORT, 2, 2, TRUE};
VARIABLE_TYPE * VARIABLE_TYPE_UINT   = &(VARIABLE_TYPE) {TY_INT, 4, 4, TRUE};
VARIABLE_TYPE * VARIABLE_TYPE_ULONG  = &(VARIABLE_TYPE) {TY_LONG, 8, 8, TRUE};

VARIABLE_TYPE * VARIABLE_TYPE_FLOAT   = &(VARIABLE_TYPE) {TY_FLOAT, 4, 4};
VARIABLE_TYPE * VARIABLE_TYPE_DOUBLE  = &(VARIABLE_TYPE) {TY_DOUBLE, 8, 8};
VARIABLE_TYPE * VARIABLE_TYPE_LDOUBLE = &(VARIABLE_TYPE) {TY_LDOUBLE, 16, 16};

/**
 * @brief Return a variable type based on the token stack
 *
 * @param PtokenStack the token stack containing type tokens
 * @return VARIABLE_TYPE * pointer to the resolved variable type
 */
VARIABLE_TYPE *
HandleType(PSCRIPT_ENGINE_TOKEN_LIST PtokenStack)
{
    enum
    {
        ENUM_VOID     = 1 << 0,
        ENUM_BOOL     = 1 << 2,
        ENUM_CHAR     = 1 << 4,
        ENUM_SHORT    = 1 << 6,
        ENUM_INT      = 1 << 8,
        ENUM_LONG     = 1 << 10,
        ENUM_FLOAT    = 1 << 12,
        ENUM_DOUBLE   = 1 << 14,
        ENUM_OTHER    = 1 << 16,
        ENUM_SIGNED   = 1 << 17,
        ENUM_UNSIGNED = 1 << 18,
    };

    VARIABLE_TYPE *      Result   = VARIABLE_TYPE_UNKNOWN;
    int                  Counter  = 0;
    PSCRIPT_ENGINE_TOKEN TopToken = NULL;

    while (PtokenStack->Pointer > 0)
    {
        TopToken = Pop(PtokenStack);
        if (TopToken->Type != SCRIPT_VARIABLE_TYPE)
        {
            Push(PtokenStack, TopToken);
            break;
        }
        if (!strcmp(TopToken->Value, "void"))
        {
            Counter += ENUM_VOID;
        }
        else if (!strcmp(TopToken->Value, "bool"))
        {
            Counter += ENUM_BOOL;
        }
        else if (!strcmp(TopToken->Value, "char"))
        {
            Counter += ENUM_CHAR;
        }
        else if (!strcmp(TopToken->Value, "short"))
        {
            Counter += ENUM_SHORT;
        }
        else if (!strcmp(TopToken->Value, "int"))
        {
            Counter += ENUM_INT;
        }
        else if (!strcmp(TopToken->Value, "long"))
        {
            Counter += ENUM_LONG;
        }
        else if (!strcmp(TopToken->Value, "float"))
        {
            Counter += ENUM_FLOAT;
        }
        else if (!strcmp(TopToken->Value, "double"))
        {
            Counter += ENUM_DOUBLE;
        }
        else if (!strcmp(TopToken->Value, "signed"))
        {
            Counter |= ENUM_SIGNED;
        }
        else if (!strcmp(TopToken->Value, "unsigned"))
        {
            Counter |= ENUM_UNSIGNED;
        }
        else
        {
            return VARIABLE_TYPE_UNKNOWN;
        }
        RemoveToken(&TopToken);

        switch (Counter)
        {
        case ENUM_VOID:
            Result = VARIABLE_TYPE_VOID;
            break;
        case ENUM_BOOL:
            Result = VARIABLE_TYPE_BOOL;
            break;
        case ENUM_CHAR:
        case ENUM_SIGNED + ENUM_CHAR:
            Result = VARIABLE_TYPE_CHAR;
            break;
        case ENUM_UNSIGNED + ENUM_CHAR:
            Result = VARIABLE_TYPE_UCHAR;
            break;
        case ENUM_SHORT:
        case ENUM_SHORT + ENUM_INT:
        case ENUM_SIGNED + ENUM_SHORT:
        case ENUM_SIGNED + ENUM_SHORT + ENUM_INT:
            Result = VARIABLE_TYPE_INT;
            break;
        case ENUM_UNSIGNED + ENUM_SHORT:
        case ENUM_UNSIGNED + ENUM_SHORT + ENUM_INT:
            Result = VARIABLE_TYPE_USHORT;
            break;
        case ENUM_INT:
        case ENUM_SIGNED:
        case ENUM_SIGNED + ENUM_INT:
            Result = VARIABLE_TYPE_INT;
            break;
        case ENUM_UNSIGNED:
        case ENUM_UNSIGNED + ENUM_INT:
            Result = VARIABLE_TYPE_UINT;
            break;
        case ENUM_LONG:
        case ENUM_LONG + ENUM_INT:
        case ENUM_LONG + ENUM_LONG:
        case ENUM_LONG + ENUM_LONG + ENUM_INT:
        case ENUM_SIGNED + ENUM_LONG:
        case ENUM_SIGNED + ENUM_LONG + ENUM_INT:
        case ENUM_SIGNED + ENUM_LONG + ENUM_LONG:
        case ENUM_SIGNED + ENUM_LONG + ENUM_LONG + ENUM_INT:
            Result = VARIABLE_TYPE_LONG;
            break;
        case ENUM_UNSIGNED + ENUM_LONG:
        case ENUM_UNSIGNED + ENUM_LONG + ENUM_INT:
        case ENUM_UNSIGNED + ENUM_LONG + ENUM_LONG:
        case ENUM_UNSIGNED + ENUM_LONG + ENUM_LONG + ENUM_INT:
            Result = VARIABLE_TYPE_ULONG;
            break;
        case ENUM_FLOAT:
            Result = VARIABLE_TYPE_FLOAT;
            break;
        case ENUM_DOUBLE:
            Result = VARIABLE_TYPE_DOUBLE;
            break;
        case ENUM_LONG + ENUM_DOUBLE:
            Result = VARIABLE_TYPE_LDOUBLE;
            break;
        }
    }
    return Result;
}

/**
 * @brief Returns the common variable type between two types
 *
 * @param Ty1 the first variable type
 * @param Ty2 the second variable type
 * @return VARIABLE_TYPE * pointer to the common variable type
 */
VARIABLE_TYPE *
GetCommonVariableType(VARIABLE_TYPE * Ty1, VARIABLE_TYPE * Ty2)
{
    // if (Ty1->Kind == TY_ARRAY)
    //{
    //     return Ty1;
    // }

    // if (Ty2->Kind == TY_ARRAY)
    //{
    //     return Ty2;
    // }

    return VARIABLE_TYPE_LONG;
}
