#ifndef PAPIIO_H
#define PAPIIO_H
#include <llvm/IR/Function.h>
#include <llvm/Pass.h>
using namespace llvm;

namespace DashTracer
{
    void Annotate(Function *F);
}

namespace DashTracer
{
    namespace Passes
    {
        struct PapiIO : public ModulePass
        {
            static char ID;
            PapiIO() : ModulePass(ID) {}
            bool runOnModule(Module &M) override;
            void getAnalysisUsage(AnalysisUsage &AU) const override;
            bool doInitialization(Module &M) override;
        };
    } // namespace Passes
} // namespace DashTracer

#endif