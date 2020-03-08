#include "tik/TikHeader.h"
#include "tik/Exceptions.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/Support/Casting.h>

using namespace llvm;

std::string getCType(llvm::Type *param)
{
    std::string retType;
    if (param->isVoidTy())
    {
        return "void";
    }
    else if (param->isHalfTy())
    {
        return "half";
    }
    else if (param->isFloatTy())
    {
        return "float";
    }
    else if (param->isDoubleTy())
    {
        return "double";
    }
    else if (param->isX86_FP80Ty())
    {
        return "long double";
    }
    else if (param->isFP128Ty())
    {
        return "__float128";
    }
    else if (param->isIntegerTy(8))
    {
        return "uint8_t";
    }
    else if (param->isIntegerTy(16))
    {
        return "uint16_t";
    }
    else if (param->isIntegerTy(32))
    {
        return "uint32_t";
    }
    else if (param->isIntegerTy(64))
    {
        return "uint64_t";
    }
    else if (param->isPointerTy())
    {
        llvm::PointerType *newType = dyn_cast<llvm::PointerType>(param);
        llvm::Type *memberType = newType->getElementType();
        return getCType(memberType) + "*";
    }
    else
    {
        if (param->isArrayTy())
        {
            throw TikException("Array argument types are not supported for header generation.");
        }
        else if (param->isVectorTy())
        {
            throw TikException("Vector argument types are not supported for header generation.");
        }
        else if (param->isStructTy())
        {
            throw TikException("Struct argument types are not supported for header generation.");
        }
        else if (param->isFunctionTy())
        {
            throw TikException("Function argument types are not supported for header generation.");
        }
        else
        {
            throw TikException("Unrecognized argument type is not supported for header generation.");
        }
    }
}
