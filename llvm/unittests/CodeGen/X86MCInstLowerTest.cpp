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
    StringRef AssemblyF = "define void @f() { ret void }";

    Triple TargetTriple("x86--");
    std::string Error;
    const Target *T = TargetRegistry::lookupTarget("", TargetTriple, Error);
    if (!T)
      GTEST_SKIP();

    TargetOptions Options;
    TM = std::unique_ptr<TargetMachine>(
        T->createTargetMachine(TargetTriple, "", "", Options, std::nullopt,
                               std::nullopt, CodeGenOptLevel::Default));
    if (!TM)
      GTEST_SKIP();

    SMDiagnostic SMError;

    Mf = parseAssemblyString(AssemblyF, SMError, Context);
    if (!Mf)
      report_fatal_error(SMError.getMessage());
    Mf->setDataLayout(TM->createDataLayout());

    F = Mf->getFunction("f");
    if (!F)
      report_fatal_error("F?");

    MachineModuleInfo MMIf(TM.get());
    MFf = std::make_unique<MachineFunction>(*F, *TM, *TM->getSubtargetImpl(*F),
                                            MMIf.getContext(), 0);

    MachineModuleInfo MMIg(TM.get());
    MFg = std::make_unique<MachineFunction>(*F, *TM, *TM->getSubtargetImpl(*F),
                                            MMIg.getContext(), 0);

    // MC.reset(new MCContext(TargetTriple, TM->getMCAsmInfo(),
    // TM->getMCRegisterInfo(),
    //                        TM->getMCSubtargetInfo()));

    // TM->getObjFileLowering()->Initialize(*MC, *TM);
    // MC->setObjectFileInfo(TM->getObjFileLowering());

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

  Function *F;
  std::unique_ptr<MachineFunction> MFf;
  Function *G;
  std::unique_ptr<MachineFunction> MFg;

  std::unique_ptr<TestAsmPrinter> TestPrinter;
};

TEST_F(X86MCInstLowerTest, moExternalSymbol_MCSYMBOL) {}

} // end namespace llvm
