#include "clang/AST/ASTConsumer.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Frontend/CompilerInstance.h"
#include "clang/Frontend/FrontendAction.h"
#include "clang/Tooling/Tooling.h"

using namespace clang;

using namespace std;
#include <iostream>

class FindCFGConsumer : public ASTConsumer {
public:
    explicit FindCFGConsumer(ASTContext *Context)
		{ }

    virtual void HandleTranslationUnit(ASTContext &Context) {
        myCFG = CFG::buildCFG(inFile, getBody(), *Context, cfgBuildOptions);
    }
}


class FindCFGFrontendAction : public clang::ASTFrontendAction {
public:
  virtual clang::ASTConsumer *CreateASTConsumer(
    clang::CompilerInstance &Compiler, llvm::StringRef InFile) {
    return new FindCFGConsumer(&Compiler.getASTContext());
  }
};

int main(int argc, const char **argv) {
    cout << "This is my clang tool!" << endl;
    clang::tooling::runToolOnCode(new FindCFGFrontendAction, argv[1]);
}

