// SPDX-License-Identifier: AGPL-3.0-only
// ForceInline: an LLVM pass plugin for the static build's full LTO. Every
// function the program defines that is not part of a recursion is marked
// alwaysinline (its noinline dropped), and the always-inliner then inlines
// every direct call to it -- whatever it costs, not as far as a threshold
// says. Indirect calls and recursion stay calls, and so does what LLVM
// cannot inline at all (va_start, returns_twice; isInlineViable says).
//
// Loaded by lld: -Wl,--load-pass-plugin=ForceInline.so; it puts itself at
// the start of the full-LTO pipeline, where the whole program -- musl,
// libc++, every library -- is one module.
#include <llvm/ADT/SCCIterator.h>
#include <llvm/Analysis/CallGraph.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/PassManager.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Passes/PassPlugin.h>
#include <llvm/Transforms/IPO/AlwaysInliner.h>

namespace {

struct ForceInline : llvm::PassInfoMixin<ForceInline> {
  llvm::PreservedAnalyses run(llvm::Module &module, llvm::ModuleAnalysisManager &) {
    llvm::CallGraph graph(module);
    unsigned marked = 0;
    for (auto scc = llvm::scc_begin(&graph); !scc.isAtEnd(); ++scc) {
      // A cycle -- a function calling itself, or functions calling each
      // other round -- is recursion: left as calls.
      if (scc.hasCycle())
        continue;
      for (llvm::CallGraphNode *node : *scc) {
        llvm::Function *function = node->getFunction();
        if (function == nullptr || function->isDeclaration() ||
            function->hasFnAttribute(llvm::Attribute::OptimizeNone))
          continue;
        function->removeFnAttr(llvm::Attribute::NoInline);
        function->addFnAttr(llvm::Attribute::AlwaysInline);
        ++marked;
      }
    }
    llvm::errs() << "ForceInline: " << marked << " functions marked alwaysinline\n";
    return llvm::PreservedAnalyses::none();
  }
  static bool isRequired() { return true; }
};

} // namespace

extern "C" LLVM_ATTRIBUTE_WEAK llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "ForceInline", "1", [](llvm::PassBuilder &builder) {
            builder.registerFullLinkTimeOptimizationEarlyEPCallback(
                [](llvm::ModulePassManager &passes, llvm::OptimizationLevel) {
                  passes.addPass(ForceInline());
                  passes.addPass(llvm::AlwaysInlinerPass(/*InsertLifetimeIntrinsics=*/true));
                });
          }};
}
