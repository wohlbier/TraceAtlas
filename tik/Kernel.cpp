#include "Kernel.h"
#include "Tik.h"
#include "Util.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <llvm/IR/CFG.h>
#include <llvm/IR/GlobalValue.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/ADT/SmallVector.h>

using namespace llvm;
using namespace std;

static int KernelUID = 0;

Kernel::Kernel(std::vector<int> basicBlocks, Module *M)
{
    MemoryRead = NULL;
    MemoryWrite = NULL;
    Body = NULL;
    Init = NULL;
    Conditional = NULL;
    Exit = NULL;
    Name = "Kernel_" + to_string(KernelUID++);

    FunctionType *mainType = FunctionType::get(Type::getVoidTy(TikModule->getContext()), false);
    KernelFunction = Function::Create(mainType, GlobalValue::LinkageTypes::ExternalLinkage, Name, TikModule);
    Init = BasicBlock::Create(TikModule->getContext(), "Init", KernelFunction);
    Body = BasicBlock::Create(TikModule->getContext(), "Body", KernelFunction);
    Exit = BasicBlock::Create(TikModule->getContext(), "Exit", KernelFunction);

    //start by getting a reference to all the blocks
    vector<BasicBlock *> blocks;
    for (Module::iterator F = M->begin(), E = M->end(); F != E; ++F)
    {
        for (Function::iterator BB = F->begin(), E = F->end(); BB != E; ++BB)
        {
            BasicBlock *b = cast<BasicBlock>(BB);
            string blockName = b->getName();
            uint64_t id = std::stoul(blockName.substr(7));
            if (find(basicBlocks.begin(), basicBlocks.end(), id) != basicBlocks.end())
            {
                blocks.push_back(b);
            }
        }
    }

    //get conditional logic
    GetLoopInsts(blocks);

    //now get the actual body
    GetBodyInsts(blocks);

    GetInitInsts(blocks);

    GetExits(blocks);

    //now get the memory array
    GetMemoryFunctions();

    CreateExitBlock();

    // connect Init->Loop->Body with branches
    ConnectFunctions();

   AddInputArguments();
    //remap everything so that our values resolved
    Remap();

}

nlohmann::json Kernel::GetJson()
{
    nlohmann::json j;
    if (Body != NULL)
    {
        j["Body"] = GetStrings(Body);
    }
    if (Init != NULL)
    {
        j["Init"] = GetStrings(Init);
    }
    if (MemoryRead != NULL)
    {
        j["MemoryRead"] = GetStrings(MemoryRead);
    }
    if (MemoryWrite != NULL)
    {
        j["MemoryWrite"] = GetStrings(MemoryWrite);
    }
    if (Conditional != NULL)
    {
        j["Loop"] = GetStrings(Conditional);
    }
    return j;
}

Kernel::~Kernel()
{
    if (MemoryRead != NULL)
    {
        delete MemoryRead;
    }
    if (MemoryWrite != NULL)
    {
        delete MemoryWrite;
    }
    if (Body != NULL)
    {
        delete Body;
    }
    delete Conditional;
    delete KernelFunction;
}

void Kernel::Remap()
{ 


    for (Function::iterator BB = KernelFunction->begin(), E = KernelFunction->end(); BB != E; ++BB)
    {
        for (BasicBlock::iterator BI = BB->begin(), BE = BB->end(); BI != BE; ++BI)
        {
            Instruction *inst = cast<Instruction>(BI);
            RemapInstruction(inst, VMap, llvm::RF_None);
        }
    }
}

void Kernel::AddInputArguments(void)
{
    // fourth, find the global pointers to store the input args into
    // for each global variable not used in the body function, but is used in the mem functions, those need to be initialized by input args
    // 1.) make a list of all load/store instructions in the kernel function, mread and mwrite
    // 2.) for each load instruction in mread/mwrite
    //       for each store instruction in body
    //          if the pointer operand in an mread/mwrite loadInst is found in a body storeInst, remove it from the list and break
    // Now we have our list of globals to store input args into

    // first, go through all the variables in this function and get all my input arguments required (thats above)
    // second, create a new function for the entire tik representation, with the proper inputs to initialize each global
    std::vector< llvm::Type* > inputArgs;
    for( auto inst : toAdd )
    {
        inputArgs.push_back( inst->getType() );
    }
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), inputArgs, false);
    llvm::Function* newFunc = llvm::Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, KernelFunction->getName()+"_Reformatted", TikModule);
    
    // third, clone the old function into the new one
    SmallVector< ReturnInst*, 10 > retinstlist;
    llvm::CloneFunctionInto(newFunc, KernelFunction, VMap, false, retinstlist );

    // remove the old function from the parent but do not erase it
    //KernelFunction->removeFromParent();
    //std::cout << (newFunc->getParent()->getName() == KernelFunction->getParent()->getName()) << std::endl;
    KernelFunction = newFunc;


    //std::vector< std::pair< llvm::Value*, llvm::GlobalValue*> > inputArg;
    // 1.)
    std::vector< llvm::LoadInst* > MfuncLoads;
    std::vector< llvm::StoreInst* >BfuncStores;
    for( Function::iterator KF = KernelFunction->begin(), KFE = KernelFunction->end(); KF != KFE; KF++ )
    {
        BasicBlock* block = cast<BasicBlock>(KF);
        for( BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; BI++ )
        {
            Instruction* bStore = cast<Instruction>(BI);
            if( StoreInst* sInst = dyn_cast<StoreInst>(bStore) )
            {
                BfuncStores.push_back(sInst);
            }
        }
    }
    //PrintVal( dyn_cast<LoadInst>(cast<Instruction>(MemoryRead->begin()->begin())) );
    for( Function::iterator MRB = MemoryRead->begin(), MRE = MemoryRead->end(); MRB != MRE; MRB++ )
    {
        BasicBlock* block = cast<BasicBlock>(MRB);
        for( BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; BI++ )
        {
            Instruction* mLoad = cast<Instruction>(BI);
            if( LoadInst* lInst = dyn_cast<LoadInst>(mLoad) )
            {
                if( std::find( MfuncLoads.begin(), MfuncLoads.end(), lInst) == MfuncLoads.end() )
                {

                    MfuncLoads.push_back(lInst);
                }
            }
        }
    }
    for( Function::iterator MWB = MemoryWrite->begin(), MWE = MemoryWrite->end(); MWB != MWE; MWB++ )
    {
        BasicBlock* block = cast<BasicBlock>(MWB);
        for( BasicBlock::iterator BI = block->begin(), BE = block->end(); BI != BE; BI++ )
        {
            Instruction* mLoad = cast<Instruction>(BI);
            if( LoadInst* lInst = dyn_cast<LoadInst>(mLoad) )
            {
                if( std::find( MfuncLoads.begin(), MfuncLoads.end(), lInst) == MfuncLoads.end() )
                {                
                    MfuncLoads.push_back(lInst);
                }
            }
        }
    }
    /*for( auto i : BfuncStores )
    {
        PrintVal(i);
    }
    for( auto i : MfuncLoads)
    {
        PrintVal(i);
    }*/

    // 2.)
    // First, make lists of all load and store instructions to be removed
    std::vector< llvm::StoreInst* > toRemoveB;
    std::vector< llvm::LoadInst* > toRemove;
    for( auto bStore : BfuncStores )
    {
        for( int i = 0; i < MfuncLoads.size(); i++ )// mLoad : MfuncLoads )
        {
            if( bStore->getPointerOperand() == MfuncLoads[i]->getPointerOperand() )
            {
                toRemoveB.push_back( bStore );
                toRemove.push_back( MfuncLoads[i] );
            }
            for( int j = i+1; j < MfuncLoads.size(); j++ )// remove : MfuncLoads )
            {
                if( MfuncLoads[j]->getPointerOperand() == MfuncLoads[i]->getPointerOperand() )
                {
                    if( std::find( toRemove.begin(), toRemove.end(), MfuncLoads[j] ) == toRemove.end() )
                    {
                        //std::cout << "removing duplicate..." << std::endl;
                        //PrintVal( MfuncLoads[j]->getPointerOperand());
                        toRemove.push_back( MfuncLoads[j] );
                        break;
                    }
                }
            }
        }
    }

    // Second, delete the ones that don't belong
    for( int i = 0; i < toRemove.size(); i++)//auto lInst : toRemove)
    {
        MfuncLoads.erase( std::find( MfuncLoads.begin(), MfuncLoads.end(), toRemove[i] ) );
    }
    for( int i = 0; i < toRemoveB.size(); i++ )
    {
        BfuncStores.erase( std::find( BfuncStores.begin(), BfuncStores.end(), toRemoveB[i] ) );
    }
    /*for( auto i : BfuncStores )
    {
        PrintVal(i);
    }
    for( auto i : MfuncLoads)
    {
        PrintVal(i);
    }*/

    // fifth, store the input arguments to my globals
    llvm::BasicBlock* newInit = NULL;
    for( iplist<BasicBlock>::iterator block = KernelFunction->getBasicBlockList().begin(); block != KernelFunction->getBasicBlockList().end(); block++ )
    {
        if( block->getName() == "Init")
        {
            newInit = block->getSingleSuccessor()->getSinglePredecessor();//->getBlock();  //std::find( KernelFunction->getBasicBlockList().begin(), KernelFunction->getBasicBlockList().end(), block );
            break;
        }
    }

    std::vector< llvm::StoreInst* > newStores;
    auto initList = &newInit->getInstList();
    for( int i = 0; i < MfuncLoads.size(); i++ )
    {
        for( auto inarg = KernelFunction->arg_begin(), inargE = KernelFunction->arg_end(); inarg != inargE; inarg++ )
        {
            if( inarg->getType() == MfuncLoads[i]->getType() )
            {
                // we have a candidate. now check if we have already assigned this arg
                bool taken = false;
                for( auto sInst : newStores )//std::find( newStores.begin(), newStores.end(), dyn_cast<StoreInst>(inarg) ) == newStores.end() )
                {
                    if( sInst->getValueOperand() == inarg)
                    {
                        taken=true;
                    }
                }
                if( !taken )
                {
                    IRBuilder<> builder( Init );//i->getNextNode() );
                    //Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
                    // now store the input arg in our global pointer
                    //PrintVal(MfuncLoads[i]);
                    auto b = builder.CreateStore( inarg, MfuncLoads[i]->getPointerOperand() );
                    newStores.push_back(b);
                    //PrintVal(b);
                    // add it to the instruction list of Init
                    //initList->push_front(b);
                    break;
                }
            }
        }
    }

    for( auto add : toAdd )
    {
        for( auto store : newStores )
        {
            //if( add->getPointerOperand() == store->getPointerOperand() )
            //{
                VMap[add] = store;
                //break;
            //}
        }
    }
    std::cout << "Finished inargs." << std::endl;










    // get a pointer to the instruction list for the Init function (declared globally on top)
/*    auto initList = &Init->getInstList();
    for (auto init : toAdd)
    {
        // find the global variable this instruction refers to
        auto index = GlobalMap.find( init );
        if( index == GlobalMap.end() )
        {
            llvm::Constant *globalInt = ConstantPointerNull::get( cast<PointerType>(init->getType()) );
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[ init ] = g;
            index = GlobalMap.find(init);
        }
        PrintVal(index->second);
        Instruction *i = init->clone();
        IRBuilder<> builder( Init );//i->getNextNode() );
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        //PrintVal(a->getType());
        // now store the input arg in our global pointer
        auto b = builder.CreateStore(i, index->second);
        VMap[init] = b;
        // add it to the instruction list of Init
        initList->push_back(b);
        //initList->push_back(i);
    }
*/
/*
    // first, go through all the variables in this function and get all my input arguments required (thats above)
    // second, create a new function for the entire tik representation, with the proper inputs to initialize each global
    std::vector< llvm::Type* > inputArgs;
    for( auto inst : toAdd )
    {
        inputArgs.push_back( inst->getType() );
    }
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), inputArgs, false);
    llvm::Function* newFunc = llvm::Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, KernelFunction->getName()+"_Reformatted", TikModule);
    
    // third, clone the old function into the new one
    SmallVector< ReturnInst*, 10 > retinstlist;
    llvm::CloneFunctionInto(newFunc, KernelFunction, VMap, false, retinstlist );

    // remove the old function from the parent but do not erase it
    KernelFunction->removeFromParent();
    //std::cout << (newFunc->getParent()->getName() == KernelFunction->getParent()->getName()) << std::endl;
    KernelFunction = newFunc;*/
}

void Kernel::GetLoopInsts(vector<BasicBlock *> blocks)
{
    // we want to detect all instructions that are responsible for deciding whether or not the kernel should continue
    // to do this we look for all instructions that are terminators, and successors to those terminators, 
    //     and decide whether or not they are indeed maintaining the loop
    // terminators are a special class of instructions in LLVM IR
    Conditional = BasicBlock::Create(TikModule->getContext(), "Loop", KernelFunction);
    vector<Instruction *> result;
    // vector of unique basic blocks belonging to the kernel
    vector<BasicBlock *> exits;
    // identify all exit blocks
    // an exit block is one that is supposed to exit a loop
    for (BasicBlock *block : blocks)
    {
        bool exit = false;
        Instruction *term = block->getTerminator();
        for (int i = 0; i < term->getNumSuccessors(); i++)
        {
            BasicBlock *succ = term->getSuccessor(i);
            // if the successor to this terminator is not in the set of kernel blocks, its not in this kernel
            if (find(blocks.begin(), blocks.end(), succ) == blocks.end())
            {
                exit = true;
            }
        }
        // if the successor is in this kernel, add that block to the kernel
        if (exit)
        {
            exits.push_back(block);
        }
    }

    //now that we have the exit blocks we can get the conditional logic for them
    assert(exits.size() == 1);
    // vector of unique terminator instructions that are based on a condition
    std::vector<Instruction *> conditions;
    for (BasicBlock *exit : exits)
    {
        Instruction *inst = exit->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(inst))
        {
            if (brInst->isConditional())
            {
                Instruction *condition = cast<Instruction>(brInst->getCondition());
                // assign this to the global value to we can access it globally when connecting Loop to Body
                cond = condition;
                conditions.push_back(condition);
            }
            else
            {
                throw 2;
            }
        }
        else
        {
            throw 2;
        }
    }
    assert(conditions.size() == 1);

    // now that we have conditional terminator instructions, look for their successors
    while (conditions.size() != 0)
    {
        Instruction *check = conditions.back();
        conditions.pop_back();
        bool elegible = true;
        //a terminator instruction is eligible only if all of its users are in the loop
        // users are all the instructions that consume the value produced by this terminator instruction
        for (auto user : check->users())
        {
            Instruction *usr = cast<Instruction>(user);
            // if this successor (usr) to the terminator (check) is not in our result already
            if (find(result.begin(), result.end(), usr) == result.end() && result.size())
            {
                // if this successor is a branch  
                if (BranchInst *br = dyn_cast<BranchInst>(usr))
                {
                }
                else
                {
                    elegible = false;
                }
            }
        }
        // if our successor is eligible, put the terminator in result
        if (elegible)
        {
            result.push_back(check);
            // see if the operands in our terminator are resolved in this kernel or not
            int opCount = check->getNumOperands();
            for (int i = 0; i < opCount; i++)
            {
                Value *opValue = check->getOperand(i);
                if (Instruction *op = dyn_cast<Instruction>(opValue))
                {
                    //assuming it is an instruction we should check it
                    conditions.push_back(op);
                }
            }
        }
    }

    // put all the valid terminator instructions in the Init Function
    reverse(result.begin(), result.end());
    auto condList = &Conditional->getInstList();
    for (auto cond : result)
    {
        Instruction *cl = cond->clone();
        VMap[cond] = cl;
        condList->push_back(cl);
    }
}

void Kernel::GetBodyInsts(vector<BasicBlock *> blocks)
{
    // we have to pick out the basic blocks that are doing the actual work of the kernels
    // we do this by filtering out entry blocks (blocks that don't have predecessors)
    // then we look at those block's successors
    std::vector<Instruction *> result;
    vector<BasicBlock *> entrances;
    for (BasicBlock *block : blocks)
    {
        for (BasicBlock *pred : predecessors(block))
        {
            if (find(blocks.begin(), blocks.end(), pred) == blocks.end())
            {
                //this is an entry block
                entrances.push_back(block);
            }
        }
    }
    assert(entrances.size() == 1);

    //this code won't support loops/internal kernels initially
    //!! I'm serious
    BasicBlock *currentBlock = entrances[0];
    vector<BasicBlock *> exploredBlocks;
    while (true)
    {
        // if we are on our final basic block, we're done
        if (find(blocks.begin(), blocks.end(), currentBlock) == blocks.end())
        {
            break;
        }
        string blockName = currentBlock->getName();
        uint64_t id = std::stoul(blockName.substr(7));
        if (KernelMap.find(id) != KernelMap.end())
        {
            Kernel *k = KernelMap[id];
            CallInst *newCall = CallInst::Create(k->KernelFunction);
            exploredBlocks.push_back(currentBlock);
            result.push_back(newCall);
            currentBlock = k->ExitTarget;
        }
        else
        {
            exploredBlocks.push_back(currentBlock);
            for (BasicBlock::iterator bi = currentBlock->begin(), be = currentBlock->end(); bi != be; bi++)
            {
                Instruction *inst = cast<Instruction>(bi);
                if (inst->isTerminator())
                {
                    //we should ignore for the time being
                }
                else
                {
                    //we now check if the instruction is already present
                    if (VMap.find(inst) == VMap.end())
                    {
                        Instruction *cl = inst->clone();
                        VMap[inst] = cl;
                        result.push_back(cl);
                    }
                }
            }
            Instruction *term = currentBlock->getTerminator();
            if (BranchInst *brInst = dyn_cast<BranchInst>(term))
            {
                if (brInst->isConditional())
                {
                    bool explored = true;
                    //this indicates either the exit or a for loop within the code
                    //aka a kernel or something to unroll
                    int sucNum = brInst->getNumSuccessors();
                    for (int i = 0; i < sucNum; i++)
                    {
                        BasicBlock *succ = brInst->getSuccessor(i);
                        if (find(blocks.begin(), blocks.end(), succ) != blocks.end())
                        {
                            //this is a branch to a kernel block
                            if (find(exploredBlocks.begin(), exploredBlocks.end(), succ) == exploredBlocks.end())
                            {
                                //and we haven't explored it yet
                                explored = false;
                                currentBlock = succ;
                            }
                        }
                    }
                    if (explored)
                    {
                        break;
                    }
                }
                else
                {
                    BasicBlock *succ = brInst->getSuccessor(0);
                    if (find(exploredBlocks.begin(), exploredBlocks.end(), succ) == exploredBlocks.end())
                    {
                        currentBlock = succ;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            else
            {
                throw 2;
            }
        }
    }
    auto instList = &Body->getInstList();
    for (Instruction *i : result)
    {
        instList->push_back(i);
    }
}

void Kernel::GetInitInsts(vector<BasicBlock *> blocks)
{
    // I need to find all operands in the kernel that are not initialized in the kernel
    // Then I need to make them globals and assign them to the input args
    // Operands are the values to the right of an instruction

    // for all instructions in a basic block
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        // get my operands of this instruction
        int numOps = inst->getNumOperands();
        // for all my (potentially unresolved) values
        for (int i = 0; i < numOps; i++)
        {
            Value *op = inst->getOperand(i);
            // dyn_cast actually checks if op is an instruction, if it is, dyn_cast will return an instruction pointer, and if not, NULL
            if (Instruction *operand = dyn_cast<Instruction>(op))
            {
                BasicBlock *parentBlock = operand->getParent();
                // if the parentBlock of the operand is not in this block, then these values will be unresolved in the TIK representation
                if (std::find(blocks.begin(), blocks.end(), parentBlock) == blocks.end())
                {
                    // if the operand is not in our vector of operands, add it
                    if (find(toAdd.begin(), toAdd.end(), operand) == toAdd.end())
                    {
                        toAdd.push_back(operand);
                    }
                }
            }
        }
    }

    for( auto init : toAdd )
    {
        VMap[ init ] = init->clone();
    }

    // for each unresolved operand, add it to the init list where they will be initialized 
    // get a pointer to the instruction list for the Init function (declared globally on top)
    /*auto initList = &Init->getInstList();
    for (auto init : toAdd)
    {
        // find the global variable this instruction refers to
        auto index = GlobalMap.find( init );
        if( index == GlobalMap.end() )
        {
            llvm::Constant *globalInt = ConstantPointerNull::get( cast<PointerType>(init->getType()) );
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[ init ] = g;
            index = GlobalMap.find(init);
        }
        Instruction *i = init->clone();
        IRBuilder<> builder( Init );//i->getNextNode() );
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        PrintVal(i);
        auto b = builder.CreateStore(i, index->second);
        VMap[init] = b;
        // add it to the instruction list of Init
        initList->push_back(b);
        //initList->push_back(i);
    }*/
/*
    for( BasicBlock::iterator BI = Init->begin(), BE = Init->end(); BI != BE; ++BI )
    {
        // search each instruction for each key in GlobalMap
        Instruction* inst = cast<Instruction>(BI);
        IRBuilder<> builder( inst->getNextNode() );
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        auto a = builder.CreateGEP( inst->getType(), GlobalMap[inst], constant );
        //PrintVal(a->getType());
        auto b = builder.CreateStore(inst, a);
        //globalSet.insert(b);
        //PrintVal(b);
    }
*/
}

void Kernel::GetMemoryFunctions()
{
    // load and store instruction sets, to hold the instructions for each function
    set<LoadInst *> loadInst;
    set<StoreInst *> storeInst;

    // parse through all basic block instructions and look for load and store insts, put them in our sets
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
        {
            loadInst.insert(newInst);
        }
        else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
        {
            storeInst.insert(newInst);
        }
    }

    // values to put into load and store instructions
    set<Value *> loadValues;
    set<Value *> storeValues;
    Type *memType;

    // for each mem instruction, grab its pointers and put them into our sets
    for (LoadInst *load : loadInst)
    {
        Value *loadVal = load->getPointerOperand();
        loadValues.insert(loadVal);
        Type *loadType = loadVal->getType();
    }
    for (StoreInst *store : storeInst)
    {
        Value *storeVal = store->getPointerOperand();
        storeValues.insert(storeVal);
    }
    FunctionType *funcType = FunctionType::get(Type::getInt32Ty(TikModule->getContext()), Type::getInt32Ty(TikModule->getContext()), false);
    MemoryRead = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryRead", TikModule);
    MemoryWrite = Function::Create(funcType, GlobalValue::LinkageTypes::ExternalLinkage, "MemoryWrite", TikModule);

    // do memread function
    int i = 0;
    BasicBlock *loadBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryRead);
    IRBuilder<> loadBuilder(loadBlock);
    Value *priorValue = NULL;

    // maps whose keys are loadValue (storeValue) and values are the name of the register in IR (the loop iterator)
    // these maps store pointers to an index
    map<Value *, Value *> loadMap;
    map<Value *, Value *> storeMap;
    for (Value *lVal : loadValues)
    {
        // every time we make a pointer to an index, do the same thing to the global map
        //PrintVal(VMap[lVal]);
        if( GlobalMap.find( VMap[lVal] ) == GlobalMap.end() )
        {
            //PrintVal(VMap[lVal]);
            llvm::Constant *globalInt = ConstantPointerNull::get( cast<PointerType>(lVal->getType()) );
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[ VMap[lVal] ] = g;
        }
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        auto a = loadBuilder.CreateGEP( lVal->getType(), GlobalMap[ VMap[lVal] ], constant );
        auto b = loadBuilder.CreateLoad( a );
        //PrintVal(lVal);
        Instruction *converted = cast<Instruction>( loadBuilder.CreatePtrToInt( b, Type::getInt32Ty( TikModule->getContext() ) ) );
        Constant *indexConstant = ConstantInt::get( Type::getInt32Ty( TikModule->getContext() ), i++ );
        loadMap[lVal] = indexConstant;
        //PrintVal(a);
        if (priorValue == NULL)
        {
            priorValue = converted;
        }
        else
        {
            std::cout << "\n";
            ICmpInst *cmpInst = cast<ICmpInst>(loadBuilder.CreateICmpEQ(MemoryRead->arg_begin(), indexConstant));
            SelectInst *sInst = cast<SelectInst>(loadBuilder.CreateSelect(cmpInst, converted, priorValue));
            priorValue = sInst;
        }
    }
    Instruction *loadRet = cast<ReturnInst>(loadBuilder.CreateRet(priorValue));

    //now do the memwrite
    i = 0;
    BasicBlock *storeBlock = BasicBlock::Create(TikModule->getContext(), "entry", MemoryWrite);
    IRBuilder<> storeBuilder(storeBlock);

    priorValue = NULL;
    for (Value *lVal : storeValues)
    {
        //PrintVal(VMap[lVal]);
        if( GlobalMap.find( VMap[lVal] ) == GlobalMap.end() )
        {
            //PrintVal(VMap[lVal]);
            llvm::Constant *globalInt = ConstantPointerNull::get( cast<PointerType>(lVal->getType()) );
            llvm::GlobalVariable *g = new GlobalVariable(*TikModule, globalInt->getType(), false, llvm::GlobalValue::LinkageTypes::ExternalLinkage, globalInt);
            GlobalMap[ VMap[lVal] ] = g;
        }
        // every time we make a pointer to an index, do the same thing to the global map
        Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
        auto a = storeBuilder.CreateGEP( lVal->getType(), GlobalMap[ VMap[lVal] ], constant );
        auto b = storeBuilder.CreateLoad( a );
        Instruction *converted = cast<Instruction>(storeBuilder.CreatePtrToInt(b, Type::getInt32Ty(TikModule->getContext())));
        Constant *indexConstant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), i++);
        if (priorValue == NULL)
        {
            priorValue = converted;
            storeMap[lVal] = indexConstant;
        }
        else
        {
            ICmpInst *cmpInst = cast<ICmpInst>(storeBuilder.CreateICmpEQ(MemoryWrite->arg_begin(), indexConstant));
            SelectInst *sInst = cast<SelectInst>(storeBuilder.CreateSelect(cmpInst, converted, priorValue));
            priorValue = sInst;
            storeMap[lVal] = indexConstant;
        }
    }
    Instruction *storeRet = cast<ReturnInst>(storeBuilder.CreateRet(priorValue));

    // set of global variables we have created
    std::set < llvm::Value* >globalSet;
    // for each instruction in memread (for example)
    for( BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI )
    {
        // search each instruction for each key in GlobalMap
        Instruction* inst = cast<Instruction>(BI);
        //PrintVal(inst);
        if( GlobalMap.find( inst ) != GlobalMap.end() )
        {
            IRBuilder<> builder( inst->getNextNode() );
            Constant *constant = ConstantInt::get(Type::getInt32Ty(TikModule->getContext()), 0);
            auto a = builder.CreateGEP( inst->getType(), GlobalMap[inst], constant );
            //PrintVal(a->getType());
            auto b = builder.CreateStore(inst, a);
            globalSet.insert(b);
            //PrintVal(b);
        }  
    }
    
    // remove instructions not belonging to parent kernel
    vector<Instruction *> toRemove;
    for (BasicBlock::iterator BI = Body->begin(), BE = Body->end(); BI != BE; ++BI)
    {
        Instruction *inst = cast<Instruction>(BI);
        if( globalSet.find(inst) != globalSet.end() )
        {
            continue;
        }
                
        IRBuilder<> builder(inst);
        if (LoadInst *newInst = dyn_cast<LoadInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryRead, loadMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newLoad = builder.CreateLoad(casted);
            newInst->replaceAllUsesWith(newLoad);
            //VMap[newInst] = newLoad;
            toRemove.push_back(newInst);
        }
        else if (StoreInst *newInst = dyn_cast<StoreInst>(inst))
        {
            auto memCall = builder.CreateCall(MemoryWrite, storeMap[newInst->getPointerOperand()]);
            auto casted = builder.CreateIntToPtr(memCall, newInst->getPointerOperand()->getType());
            auto newStore = builder.CreateStore(VMap[ newInst->getValueOperand() ], casted);
            toRemove.push_back(newInst);
        }
    }
    for (auto inst : toRemove)
    {
        inst->removeFromParent();
    }

    
}

void Kernel::GetExits(std::vector<llvm::BasicBlock *> blocks)
{
    vector<BasicBlock *> exits;
    for (auto block : blocks)
    {
        Instruction *term = block->getTerminator();
        if (BranchInst *brInst = dyn_cast<BranchInst>(term))
        {
            for (unsigned int i = 0; i < brInst->getNumSuccessors(); i++)
            {
                BasicBlock *succ = brInst->getSuccessor(i);
                if (find(blocks.begin(), blocks.end(), succ) == blocks.end())
                {
                    exits.push_back(succ);
                }
            }
        }
        else
        {
            throw 2;
        }
    }
    assert(exits.size() == 1);
    ExitTarget = exits[0];
}

void Kernel::CreateExitBlock(void)
{
    IRBuilder<> exitBuilder(Exit);
    auto a = exitBuilder.CreateRetVoid();
}

void Kernel::ConnectFunctions()
{
    // init->loop (unconditional)
    IRBuilder<> initBuilder(Init);
    auto a = initBuilder.CreateBr(Conditional);

    // loop->body (conditional)
    IRBuilder<> loopBuilder(Conditional);
    //Value* cond = inst->getOperand();
    auto b = loopBuilder.CreateCondBr(cond, Body, Exit);

    // body->loop (unconditional)
    IRBuilder<> bodyBuilder(Body);
    auto c = bodyBuilder.CreateBr(Conditional);

}

