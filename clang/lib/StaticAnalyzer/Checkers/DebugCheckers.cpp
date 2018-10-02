//==- DebugCheckers.cpp - Debugging Checkers ---------------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines checkers that display debugging information.
//
//===----------------------------------------------------------------------===//

#include "ClangSACheckers.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/Support/Process.h"

using namespace clang;
using namespace ento;

using namespace std;
#include <iostream>

//===----------------------------------------------------------------------===//
// DominatorsTreeDumper
//===----------------------------------------------------------------------===//

namespace {
class DominatorsTreeDumper : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager& mgr,
                        BugReporter &BR) const {
    if (AnalysisDeclContext *AC = mgr.getAnalysisDeclContext(D)) {
      DominatorTree dom;
      dom.buildDominatorTree(*AC);
      dom.dump();
    }
  }
};
}

void ento::registerDominatorsTreeDumper(CheckerManager &mgr) {
  mgr.registerChecker<DominatorsTreeDumper>();
}

//===----------------------------------------------------------------------===//
// LiveVariablesDumper
//===----------------------------------------------------------------------===//

namespace {
class LiveVariablesDumper : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager& mgr,
                        BugReporter &BR) const {
    if (LiveVariables* L = mgr.getAnalysis<LiveVariables>(D)) {
      L->dumpBlockLiveness(mgr.getSourceManager());
    }
  }
};
}

void ento::registerLiveVariablesDumper(CheckerManager &mgr) {
  mgr.registerChecker<LiveVariablesDumper>();
}

//===----------------------------------------------------------------------===//
// CFGViewer
//===----------------------------------------------------------------------===//

namespace {
class CFGViewer : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager& mgr,
                        BugReporter &BR) const {
    if (CFG *cfg = mgr.getCFG(D)) {
      cfg->viewCFG(mgr.getLangOpts());
    }
  }
};
}

void ento::registerCFGViewer(CheckerManager &mgr) {
  mgr.registerChecker<CFGViewer>();
}

//===----------------------------------------------------------------------===//
// CFGDumper (We are hacking this to print out basic blocks for us now)
//===----------------------------------------------------------------------===//

namespace {
class CFGDumper : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager& mgr,
                        BugReporter &BR) const {
    PrintingPolicy Policy(mgr.getLangOpts());
    Policy.TerseOutput = true;
    Policy.PolishForDeclaration = true;
    D->print(llvm::errs(), Policy);

    SourceManager &mySourceManager = mgr.getSourceManager();

    cout << "This is My basic block printer!" << endl;
    int bb_counter = 1;

    if (CFG *cfg = mgr.getCFG(D)) {
      for (clang::CFG::iterator CfgBlocks = cfg->begin(), CfgBlocksEnd = cfg->end(); CfgBlocks != CfgBlocksEnd; ++CfgBlocks) {
        cout << "CFG basic block number: " << bb_counter << endl;
        bb_counter++;
        int stmt_counter = 1;
        for (clang::CFGBlock::iterator BlockElements = (*CfgBlocks)->begin(), BlockElementsEnd = (*CfgBlocks)->end(); BlockElements != BlockElementsEnd; ++BlockElements) {
          if (BlockElements->getKind() == clang::CFGElement::Statement) {
            // Only print out the first statement in each basic block.
            if (stmt_counter == 1) {
              clang::CFGStmt BlockElementStmt = BlockElements->castAs<clang::CFGStmt>();
              std::string str(BlockElementStmt.getStmt()->getStmtClassName());
              cout << "Statement number: " << stmt_counter << "; Statement class: " << str << endl;
              BlockElementStmt.getStmt()->getLocStart().dump(mySourceManager);
              cout << endl;
              BlockElementStmt.getStmt()->dump();
            }
            stmt_counter++;
          }
        }
      }
    }
  }
};
}

void ento::registerCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<CFGDumper>();
}

//===----------------------------------------------------------------------===//
// CallGraphViewer
//===----------------------------------------------------------------------===//

namespace {
class CallGraphViewer : public Checker< check::ASTDecl<TranslationUnitDecl> > {
public:
  void checkASTDecl(const TranslationUnitDecl *TU, AnalysisManager& mgr,
                    BugReporter &BR) const {
    CallGraph CG;
    CG.addToCallGraph(const_cast<TranslationUnitDecl*>(TU));
    CG.viewGraph();
  }
};
}

void ento::registerCallGraphViewer(CheckerManager &mgr) {
  mgr.registerChecker<CallGraphViewer>();
}

//===----------------------------------------------------------------------===//
// CallGraphDumper
//===----------------------------------------------------------------------===//

namespace {
class CallGraphDumper : public Checker< check::ASTDecl<TranslationUnitDecl> > {
public:
  void checkASTDecl(const TranslationUnitDecl *TU, AnalysisManager& mgr,
                    BugReporter &BR) const {
    CallGraph CG;
    CG.addToCallGraph(const_cast<TranslationUnitDecl*>(TU));
    CG.dump();
  }
};
}

void ento::registerCallGraphDumper(CheckerManager &mgr) {
  mgr.registerChecker<CallGraphDumper>();
}


//===----------------------------------------------------------------------===//
// ConfigDumper
//===----------------------------------------------------------------------===//

namespace {
class ConfigDumper : public Checker< check::EndOfTranslationUnit > {
  typedef AnalyzerOptions::ConfigTable Table;

  static int compareEntry(const Table::MapEntryTy *const *LHS,
                          const Table::MapEntryTy *const *RHS) {
    return (*LHS)->getKey().compare((*RHS)->getKey());
  }

public:
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                                 AnalysisManager& mgr,
                                 BugReporter &BR) const {
    const Table &Config = mgr.options.Config;

    SmallVector<const Table::MapEntryTy *, 32> Keys;
    for (Table::const_iterator I = Config.begin(), E = Config.end(); I != E;
         ++I)
      Keys.push_back(&*I);
    llvm::array_pod_sort(Keys.begin(), Keys.end(), compareEntry);

    llvm::errs() << "[config]\n";
    for (unsigned I = 0, E = Keys.size(); I != E; ++I)
      llvm::errs() << Keys[I]->getKey() << " = " << Keys[I]->second << '\n';

    llvm::errs() << "[stats]\n" << "num-entries = " << Keys.size() << '\n';
  }
};
}

void ento::registerConfigDumper(CheckerManager &mgr) {
  mgr.registerChecker<ConfigDumper>();
}

//===----------------------------------------------------------------------===//
// ExplodedGraph Viewer
//===----------------------------------------------------------------------===//

namespace {
class ExplodedGraphViewer : public Checker< check::EndAnalysis > {
public:
  ExplodedGraphViewer() {}
  void checkEndAnalysis(ExplodedGraph &G, BugReporter &B,ExprEngine &Eng) const {
    Eng.ViewGraph(0);
  }
};

}

void ento::registerExplodedGraphViewer(CheckerManager &mgr) {
  mgr.registerChecker<ExplodedGraphViewer>();
}

