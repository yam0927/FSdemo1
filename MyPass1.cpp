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
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Transforms/Instrumentation.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/ModuleUtils.h"
#include "llvm/InitializePasses.h"
#include "llvm-c/Initialization.h"


#include <cstdio>
#include <iostream>
#include <thread>
#include <set>
#include <map>
using namespace llvm;

int nInstrumentIns = 0;
static const size_t nAccessSizes = 5;
static size_t typeSize2accessSizeIndex(uint64_t typeSize);
//static size_t countTrailingZeros_32(uint32_t size);
//access size:1/2/4/8/16 bytes 

namespace {

	struct MyPass1 : public FunctionPass {

    static char ID; // Pass identification, replacement for typeid
	LLVMContext *context;
	Type* intPtrType;

	std::set<Value *> stackValue;
	std::map<Value *, bool> pointerValue;

 	Function* accessCallbacks[2][nAccessSizes];// save 10 callbacks
//  Constant* accessCallbacks[2][nAccessSizes];
    MyPass1() : FunctionPass(ID) {}

    bool doInitialization(Module &M) override;
	bool doFinalization(Module &M) override;
	void initializeCallbacks(Module &M);

    Value *isInterestingMemoryAccess(Instruction *ins, bool *isWrite, uint64_t *typeSize);
    bool instrumentMemAccess(Instruction *ins, Value *line);
	bool runOnFunction(Function &F) override;
    Function* checkInterfaceFunction(Constant *FuncOrBitcast);
  };
}




//initialize accessCallback[2][5]
bool MyPass1::doInitialization(Module &M){
	context = &(M.getContext());
	auto &DL = M.getDataLayout();
	IRBuilder<> IRB(*context);
	intPtrType = IRB.getIntPtrTy(DL);
	std::cout << "Start Instrument!\n";

    return true;
}

void MyPass1::initializeCallbacks(Module &M){
	IRBuilder<> IRB(*context);
	Function *hook;
	Constant *hookFunc;
	for(size_t isWriteAccess = 0; isWriteAccess <= 1; isWriteAccess++){
		for(size_t accessSizeIndex = 0; accessSizeIndex < nAccessSizes; accessSizeIndex++){
			std::string funcName;
			if(isWriteAccess){
				funcName = std::string("store_") + itostr(1 << accessSizeIndex) + "_bytes";
			}
			else{
				funcName = std::string("load_") + itostr(1 << accessSizeIndex) + "_bytes";
			}

			hookFunc = M.getOrInsertFunction( funcName, FunctionType::get(IRB.getVoidTy(), {intPtrType, intPtrType}, false) );
    //        accessCallbacks[isWriteAccess][accessSizeIndex] = hookFunc;

     		hook = cast<Function>(hookFunc);
			accessCallbacks[isWriteAccess][accessSizeIndex] = checkInterfaceFunction( M.getOrInsertFunction( funcName, FunctionType::get(IRB.getVoidTy(), {intPtrType, intPtrType}, false) ));
		}
	}
}


Function* MyPass1::checkInterfaceFunction(Constant *FuncOrBitcast){
	if (isa<Function>(FuncOrBitcast))
		return cast<Function>(FuncOrBitcast);
}

//get memory access address(type:Value*) of all load/store instructions. 
Value *MyPass1::isInterestingMemoryAccess(Instruction *ins, bool *isWrite, uint64_t *typeSize){
	const DataLayout &DL  = ins->getModule()->getDataLayout();
    ////////////////////////////////////////////////////////////////////////
	/*
	if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(ins)){
		Value *addr = GEPI -> getPointerOperand();
		if(stackValue.find(addr) != stackValue.end()){
			stackValue.insert(ins);
			return nullptr;
		}
	}

	else if(AllocaInst *AI = dyn_cast<AllocaInst>(ins)){
		Type *allocaType = AI -> getAllocatedType();
		if(allocaType -> isPointerTy())
			pointerValue[AI] = false;
		else
			stackValue.insert(ins);
	}

	else if(LoadInst *LI = dyn_cast<LoadInst>(ins)){
		*isWrite = false;
		*typeSize = DL.getTypeStoreSizeInBits(LI->getType());
		Value *addr = LI -> getPointerOperand();
		if(stackValue.find(addr) != stackValue.end())
			stackValue.insert(LI);
		else if(pointerValue.find(addr) != pointerValue.end()){
			if(pointerValue[addr] == false){
				stackValue.insert(ins);
				return nullptr;
			}
		}
		else{
			stackValue.insert(ins);
			return addr;
		}
	}


	else if(StoreInst *SI = dyn_cast<StoreInst>(ins)){
		*isWrite = true;
		*typeSize = DL.getTypeStoreSizeInBits(SI->getValueOperand()->getType());
		Value *addr = SI -> getPointerOperand();
		if(stackValue.find(addr) == stackValue.end()){
			if(pointerValue.find(addr) != pointerValue.end()){
				Value *first_operand = SI -> getOperand(0);
				if(stackValue.find(first_operand) != stackValue.end())
					pointerValue[addr] = false;
				else
					pointerValue[addr] = true;
			}
			else
				return addr;
		}
		else
			return nullptr;

	}*/
	///////////////////////////////////////////////////////////////////
	if(GetElementPtrInst *GEPI = dyn_cast<GetElementPtrInst>(ins)){
		Value *addr = GEPI -> getPointerOperand();
		if(stackValue.find(addr) != stackValue.end()){
			stackValue.insert(ins);
		}
	}
	else if(AllocaInst *AI = dyn_cast<AllocaInst>(ins)){
		Type *allocaType = AI -> getAllocatedType();
		if(allocaType -> isPointerTy())
			pointerValue[ins] = false;
		else
			stackValue.insert(ins);
	}
	else if(LoadInst *LI = dyn_cast<LoadInst>(ins)){
		*isWrite = false;
		*typeSize = DL.getTypeStoreSizeInBits(LI->getType());
		Value *addr = LI -> getPointerOperand();
		if(stackValue.find(addr) != stackValue.end()){//load a stack variable ,save it in a temp %x(ins)
			stackValue.insert(ins);
		}//non stack variable or pointer variables
		else if(pointerValue.find(addr) != pointerValue.end()){

			if(pointerValue[addr] == false){
				stackValue.insert(ins);//load a pointer variable pointing to a stack variable, save it in a temp %x(ins)
			}
			else{//load a pointer variable pointing to a non-stack variable
				;
			}
		}
		else{//load a non-stack variable ,save it in a temp %x(ins) or load a pointer variable(non-alloc) in temp %x(ins)
			stackValue.insert(ins);
			return addr;
		}
	}
	else if(StoreInst *SI = dyn_cast<StoreInst>(ins)){
		*isWrite = true;
		*typeSize = DL.getTypeStoreSizeInBits(SI->getValueOperand()->getType());
		Value *addr = SI -> getPointerOperand();
		if(stackValue.find(addr) == stackValue.end()){
			if(pointerValue.find(addr) != pointerValue.end()){
				Value *first_operand = SI -> getOperand(0);
				if(stackValue.find(first_operand) != stackValue.end())
					pointerValue[addr] = false;
		    	else if(pointerValue.find(first_operand) != pointerValue.end()){
					if(pointerValue[first_operand] == false)
						pointerValue[addr] = false;
					else
						pointerValue[addr] = true;
				}
			}
			else
				return addr;
		}
		else
			;
	}
	///////////////////////////////////////////////////////////////////

	else if(AtomicRMWInst *RMW = dyn_cast<AtomicRMWInst>(ins)){
		*isWrite = true;
		*typeSize = DL.getTypeStoreSizeInBits(RMW->getValOperand()->getType());
		return RMW -> getPointerOperand();
	}
	else if(AtomicCmpXchgInst *XCHG = dyn_cast<AtomicCmpXchgInst>(ins)){
		*isWrite = true;
		*typeSize = DL.getTypeStoreSizeInBits(XCHG->getCompareOperand()->getType());
		return XCHG -> getPointerOperand();
	}
	else if(dyn_cast<ICmpInst>(ins) || dyn_cast<BinaryOperator>(ins) || dyn_cast<CastInst>(ins))
		stackValue.insert(ins);
	return nullptr;
}



//transfer typeSize to accessSizeIndex eg:typeSize = 32bits = 4Bytes = 0x100 Bytes, accessSizeIndex = 2 
static size_t typeSize2accessSizeIndex(uint64_t typeSize){
	size_t sizeIndex = countTrailingZeros(typeSize / 8);
	assert(sizeIndex < nAccessSizes);
	return sizeIndex;
}


bool MyPass1::instrumentMemAccess(Instruction *ins, Value *line){
	uint64_t typeSize = 0;
	bool isWrite = false;
	Value *addr, *addrLong;
	addr = isInterestingMemoryAccess(ins, &isWrite, &typeSize);
	
	if(!addr)
		return false;

	IRBuilder<> IRB(ins);
    addrLong = IRB.CreatePointerCast(addr, intPtrType);
	size_t accessSizeIndex = typeSize2accessSizeIndex(typeSize);
	IRB.CreateCall(accessCallbacks[isWrite][accessSizeIndex], {addrLong, line});

	return true;
}

bool MyPass1::runOnFunction(Function &F){
	initializeCallbacks(*F.getParent());
	bool hasChanged = false;
	SmallVector<Instruction* , 16> toInstrument;

	for(auto &BB : F){
		for(auto &Inst : BB){
			bool isWrite;
			uint64_t typeSize;
			Value *addr = isInterestingMemoryAccess(&Inst, &isWrite, &typeSize);
            
//		    const DebugLoc &location = Inst.getDebugLoc();
//          if(location){
//  		    unsigned line = location.getLine();
//		    }
			if(addr){
//				DILocation *loc = Inst.getDebugLoc();
//				if(loc){
//			    	int lineNum = loc->getLine();
//				    printf("Line Number:%d\n", lineNum);
//				    Value *line = ConstantInt::get(intPtrType, loc->getLine());
//              }
				toInstrument.push_back(&Inst);
				nInstrumentIns++;
			}
		}
	}

	for(auto Inst : toInstrument){
		DILocation *loc = Inst -> getDebugLoc();
		Value *line;
		if(loc){
			int lineNum = loc->getLine();
			printf("Line Number:%d\n", lineNum);
			line = ConstantInt::get(intPtrType, loc->getLine());
		}
		hasChanged |= instrumentMemAccess(Inst, line);
	}


	return hasChanged;
}

bool MyPass1::doFinalization(Module &M){
	std::cout << "Total Instrument ins: " << nInstrumentIns << "\n";
	return false;
}


char MyPass1::ID = 0;
static RegisterPass<MyPass1> X("MyPass1", "Test Addr Value ID Pass");

