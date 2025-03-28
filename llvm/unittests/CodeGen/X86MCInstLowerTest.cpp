//===- llvm/unittest/CodeGen/AArch64SelectionDAGTest.cpp
//-------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../lib/Target/X86/X86ISelLowering.h"
#include "TestAsmPrinter.h"
#include "llvm/Analysis/MemoryLocation.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/SelectionDAG.h"
#include "llvm/CodeGen/TargetLowering.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/KnownBits.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Target/TargetLoweringObjectFile.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace llvm {

class X86MCInstLowerTest : public testing::Test {
protected:
  static void SetUpTestCase() {
    InitializeAllTargets();
    InitializeAllTargetMCs();
  }

  bool addPassesToEmitMC(llvm::legacy::PassManagerBase &PM,
                         llvm::MachineModuleInfoWrapperPass *MMIWP) {
    // Targets may override createPassConfig to provide a target-specific
    // subclass.
    TargetPassConfig *PassConfig = TM->createPassConfig(PM);

    // Set PassConfig options provided by TargetMachine.
    PassConfig->setDisableVerify(true);
    PM.add(PassConfig);
    PM.add(MMIWP);

    if (PassConfig->addISelPasses())
      return true;

    PassConfig->addMachinePasses();
    PassConfig->setInitialized();

    return false;
  }

  void SetUp() override {
    const char *FooStr = R""""(
        @G = external global i32

        define i32 @foo() {
          %1 = load i32, i32* @G; load the global variable
          %2 = call i32 @f()
          %3 = mul i32 %1, %2
          ret i32 %3
        }

        declare i32 @f() #0
      )"""";
    StringRef AssemblyF(FooStr);

    Triple TargetTriple("x86_64--");
    std::string Error;
    const Target *T = TargetRegistry::lookupTarget("", TargetTriple, Error);
    if (!T)
      GTEST_SKIP();

    TargetOptions Options;
    TM = std::unique_ptr<TargetMachine>(T->createTargetMachine(
        TargetTriple, "", "", Options, Reloc::Model::PIC_,
        CodeModel::Model::Large, CodeGenOptLevel::Default));
    if (!TM)
      GTEST_SKIP();

    SMDiagnostic SMError;

    Mf = parseAssemblyString(AssemblyF, SMError, Context);
    if (!Mf)
      report_fatal_error(SMError.getMessage());
    Mf->setDataLayout(TM->createDataLayout());

    // F = Mf->getFunction("foo");
    // if (!F)
    //   report_fatal_error("F?");

    MCFoo.reset(new MCContext(TargetTriple, TM->getMCAsmInfo(),
                           TM->getMCRegisterInfo(), TM->getMCSubtargetInfo()));
    MCFoo->setObjectFileInfo(TM->getObjFileLowering());
    TM->getObjFileLowering()->Initialize(*MCFoo, *TM);
    MCFoo->setObjectFileInfo(TM->getObjFileLowering());

    MachineModuleInfoWrapperPass* MMIWPf = new MachineModuleInfoWrapperPass(TM.get(), &*MCFoo);
    legacy::PassManager passMgrF;
    addPassesToEmitMC(passMgrF, MMIWPf);
    passMgrF.run(*Mf);
    Mf->dump();


    auto ExpectedTestPrinter = TestAsmPrinter::create(
        TargetTriple.str(), /*DwarfVersion=*/4, dwarf::DWARF32);

    ASSERT_THAT_EXPECTED(ExpectedTestPrinter, Succeeded());
    TestPrinter = std::move(ExpectedTestPrinter.get());
  }

  LLVMContext Context;
  std::unique_ptr<TargetMachine> TM;
  std::unique_ptr<Module> Mf;
  std::unique_ptr<Module> Mg;

  std::unique_ptr<MCContext> MC;
  std::unique_ptr<MCContext> MCFoo;
  std::unique_ptr<MCContext> MCBar;

  Function *F;
  std::unique_ptr<MachineFunction> MFf;
  Function *G;
  std::unique_ptr<MachineFunction> MFg;

  std::unique_ptr<TestAsmPrinter> TestPrinter;
};

TEST_F(X86MCInstLowerTest, moExternalSymbol_MCSYMBOL) {}

} // end namespace llvm
