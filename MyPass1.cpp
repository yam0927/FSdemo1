//===- Hello.cpp - Example code from "Writing an LLVM Pass" ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements two versions of the LLVM "Hello World" pass described
// in docs/WritingAnLLVMPass.html
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/Statistic.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/InitializePasses.h"
#include "llvm-c/Initialization.h"


#include <stdio.h>
#include <iostream>
#include <thread>
using namespace llvm;

static const size_t nAccessSizes = 5;
static size_t typeSize2accessSizeIndex(uint32_t typeSize);
static size_t countTrailingZeros_32(uint32_t size);
//access size:1/2/4/8/16 bytes 

namespace {
  
  struct MyPass1 : public FunctionPass {

    static char ID; // Pass identification, replacement for typeid
	LLVMContext *context;
	DataLayout *TD;
	int longSize;
	Type* intptrType;
	Function *ctorFunction;
 	Function* accessCallback[2][nAccessSizes];// save 10 callbacks

    MyPass1() : FunctionPass(ID) {}

    virtual bool doInitialization(Module &M);
	virtual bool doFinalization(Module &M);
    Value* isInterestingMemoryAccess(Instruction *ins, bool *isWrite);
    void instrumentMemoryAccess(Instruction *ins);
    void instrumentSpecificAccess(Instruction *ins, IRBuilder<> &IRB, Value *addr, uint32_t typeSize, bool isWrite);
    Instruction* insertAccessCallback( Instruction *insertBefore, Value *addr, bool isWrite, size_t accessSizeArrayIndex);
	Function* checkInterfaceFunction(Constant *FuncOrBitcast);
    bool runOnFunction(Function &F) override {
	  errs() << "Function Name:    " << F.getName() << "\n";
//	  int pointerSize = TD -> getPointerSizeInBits();
	  for(Function::iterator bb = F.begin(), FE = F.end(); bb != FE; ++bb ){
			Value* addr = NULL;
			Type* origPtrType;
			Type* origType;
			uint32_t typeSize;
//     		Type* intptrType;
			//int pointerSize;
//			int pointerSize = TD -> getPointerSizeInBits();
//			errs() << "The size of pointer is:" << pointerSize << " bits.\n";

	  		for(BasicBlock::iterator ins = bb->begin(), BE = bb->end(); ins != BE; ++ins){
			//	if(DILocation *loc = ins->getDebugLoc()){
			//		unsigned line = loc->getLine();
			//		errs() << "Line" << line << "\n";
			//	}
				//Type(ins) is Instruction *.
				if(LoadInst *LI = dyn_cast<LoadInst>(ins)){
					errs() << "A Load instruction:\n";
					addr = LI->getPointerOperand();
					origPtrType = addr -> getType();
					origType = cast<PointerType>(origPtrType) -> getElementType();
					assert(origType->isSized());
					typeSize = TD -> getTypeStoreSizeInBits(origType);
				   // pointerSize = TD -> getPointerSizeInBits();

					errs() << "Addr is:" << addr <<"       *Addr is:" << *addr << "     typeSize is: " << typeSize << "\n";

				}
				if(StoreInst *SI = dyn_cast<StoreInst>(ins)){
					errs() << "A Store instruction:\n";
					addr = SI->getPointerOperand();
					origPtrType = addr -> getType();
					origType = cast<PointerType>(origPtrType) -> getElementType();
					assert(origType->isSized());
					typeSize = TD -> getTypeStoreSizeInBits(origType);
					errs() << "Addr is:" << addr <<"       *Addr is:" << *addr << "      typeSize is: " << typeSize << "\n";
					//std::cout << "This store instruction caused by Thread ID :" << std::this_thread::get_id() << "\n";

				}

			}
	  }
      return false;
    }
  };
}




//initialize accessCallback[2][5]
bool MyPass1::doInitialization(Module &M){
	context = &(M.getContext());
	longSize = TD->getPointerSizeInBits();
	intptrType = Type::getIntNTy(*context, longSize);

	ctorFunction = Function::Create(FunctionType::get(Type::getVoidTy(*context), false),GlobalValue::InternalLinkage, "MyPass1", &M);
	BasicBlock *ctorBB = BasicBlock::Create(*context, "", ctorFunction);
	Instruction * ctorinsertBefore = ReturnInst::Create(*context, ctorBB);
	IRBuilder<> IRB(ctorinsertBefore);

	Constant *hookFunc;
	Function *hook;
//	TD = getAnalysisIfAvailable<DataLayout>();
//	if(!TD)
//		return false;
	for(size_t isWriteAccess = 0; isWriteAccess <= 1; isWriteAccess++){
		for(size_t accessSizeIndex = 0; accessSizeIndex < nAccessSizes; accessSizeIndex++){
			std::string funcName;
			if(isWriteAccess){
				funcName = std::string("store_") + itostr(1 << accessSizeIndex) + "_bytes";
			}
			else{
				funcName = std::string("load_") + itostr(1 << accessSizeIndex) + "_bytes";

			}
		//	funcOrBitCast = M.getOrInsertFunction(funcName, IRB.getVoidTy(), intptrType, NULL);
			/* parameters of getOrInsertFunction????*/
			hookFunc = M.getOrInsertFunction(funcName, IRB.getVoidTy(), NULL);
			hook = cast<Function>(hookFunc);
			accessCallback[isWriteAccess][accessSizeIndex] = hook;
   		//	accessCallback[isWriteAccess][accessSizeIndex] = checkInterfaceFunction(M.getOrInsertFunction(funcName, IRB.getVoidTy(), intptrType, NULL));
		}
	}

}


Function* MyPass1::checkInterfaceFunction(Constant *FuncOrBitcast){
	if (isa<Function>(FuncOrBitcast))
		return cast<Function>(FuncOrBitcast);
}

//get memory access address(type:Value*) of all load/store instructions. 
Value* MyPass1::isInterestingMemoryAccess(Instruction *ins, bool *isWrite){
	if(LoadInst *LI = dyn_cast<LoadInst>(ins)){
		*isWrite = false;
		return LI -> getPointerOperand();
	}
	if(StoreInst *SI = dyn_cast<StoreInst>(ins)){
		*isWrite = true;
		return SI -> getPointerOperand();
	}
	if(AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(ins)){
		*isWrite = true;
		return RMW -> getPointerOperand();
	}
	if(AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(ins)){
		*isWrite = true;
		return XCHG -> getPointerOperand();
	}
	return NULL;
}

//instrument interesing memory access
void MyPass1::instrumentMemoryAccess(Instruction *ins){
	bool isWrite = false;
	Value* addr = isInterestingMemoryAccess(ins, &isWrite);

    //if ins is not an interesting ins, addr is NULL.
	assert(addr);
	Type* origPtrType = addr -> getType();
	Type* origType = cast<PointerType>(origPtrType) -> getElementType();

	assert(origType->isSized());
    uint32_t typeSize = TD -> getTypeStoreSizeInBits(origType);
    if(typeSize != 8 && typeSize != 16 && typeSize!= 32 && typeSize != 64 && typeSize != 128){
	    return;
	}

    IRBuilder<> IRB(ins);
    instrumentSpecificAccess(ins, IRB, addr, typeSize, isWrite);
}

//instrument specific memory access(load/store(addr != NULL) && typeSize=8/16/32/64/128 bits))
void MyPass1::instrumentSpecificAccess(Instruction *ins, IRBuilder<> &IRB, Value *addr, uint32_t typeSize, bool isWrite){
	size_t accessSizeIndex = typeSize2accessSizeIndex(typeSize);
    Value * actualAddr = IRB.CreatePointerCast(addr, intptrType);
	insertAccessCallback(ins, actualAddr, isWrite, accessSizeIndex);
}

Instruction* MyPass1::insertAccessCallback( Instruction *insertBefore, Value *addr, bool isWrite, size_t accessSizeArrayIndex){
	IRBuilder<> IRB(insertBefore);
	CallInst *call = IRB.CreateCall(accessCallback[isWrite][accessSizeArrayIndex], addr);
	return call;
}



//transfer typeSize to accessSizeIndex eg:typeSize = 32bits = 4Bytes = 0x100 Bytes, accessSizeIndex = 2 
static size_t typeSize2accessSizeIndex(uint32_t typeSize){
	size_t sizeIndex = countTrailingZeros_32(typeSize / 8);
	assert(sizeIndex < nAccessSizes);
	return sizeIndex;
}

//count the number of trailing zeros
static size_t countTrailingZeros_32(uint32_t nBytes){
    size_t nZeros = 0;
	while(nBytes != 1){
		nBytes = nBytes >> 1;
		nZeros++;
	}
	return nZeros;
}

bool MyPass1::doFinalization(Module &M){
	return false;
}

/*
bool MyPass1::runOnFunction(Function &F){
}*/



char MyPass1::ID = 0;
static RegisterPass<MyPass1> X("MyPass1", "Test Addr Value ID Pass");

