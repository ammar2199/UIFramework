#include "Expression.h"
#include "View.h"
#include <array>
#include <algorithm>
#include <string>
#include <sstream>
#include <iostream>
#include <vector>

//TODO: LATER
// - Need to remove all the random prints/logging.
// - std::unordered_map<std::string, int> Variable::VariableMap;
// 

Expression<double> Variable::operator*(double c) const {
  Expression<double> e;
  e.AddVariable(*this, c);
  return e;
}

Expression<double> Variable::operator+(const Variable& v) const {
  Expression<double> e;
  e.AddVariable(*this, 1.0);
  e.AddVariable(v, 1.0); 
  return e;
}

Variable Constraint::GetVar(const Box* const b, BoxAttribute attribute) {
  switch (attribute) {
    case BoxAttribute::Left:
      return b->GetLeftVar();
    case BoxAttribute::Right:
      return b->GetRightVar();
    case BoxAttribute::Top:
      return b->GetTopVar();
    case BoxAttribute::Bottom:
      return b->GetBottomVar();
    case BoxAttribute::Width:
      return b->GetWidthVar();
    case BoxAttribute::Height:
      return b->GetHeightVar();
  }
  return Variable();
}

std::string Tableau2::GetRep() const { 
  std::stringstream stream;
  stream << "Objective: " << mErrorObjectiveFunc << '\n';
  stream << "Rows:\n";
  int i=0;
  for (auto& pair : mRows) {
    stream << pair.first.GetName() << " = " << pair.second->GetRep();
    //if (++i < mRows.size()) stream << '\n';
    stream << '\n';
  }
  stream << "Edit Vars:\n";
  for (auto& pair : mEditVarInfoMap) {
    stream << pair.first.GetName() << ": PlusVar:" << pair.second.plusErrorVar << " MinusVar:" << pair.second.minusErrorVar << " Constant:" << pair.second.originalValue;
    if (++i < mRows.size()) stream << '\n';
  }
  return stream.str();
}

void Tableau2::AddConstraint(const Expression<double>& e1, const Relation rel, const Expression<double>& e2, unsigned int strength) {
  mSolved = false;
  assert(strength > 0 && strength <= Tableau2::REQUIRED && "AddConstraint: strength not in range E [0,1000]");
  Expression<double>* expr = FormTableauExpression(e1, rel, e2, strength);
  std::cout << "Formed-Expr: " << *expr << std::endl;
  std::vector<Variable> exprVars = expr->GetVariables();
  // Now its either Expression = 0 or Expression >= 0

  
  // Add Error Variables
  if (strength < Tableau2::REQUIRED) {
    for (auto& var : exprVars) {
      if (var.GetType() == VariableType::Error) {
        std::array<double, decltype(mErrorObjectiveFunc)::coefficient_type::num_coefficients> buff{};
        buff.fill(0.0);
        buff[buff.size() - strength]=1.0;
        mErrorObjectiveFunc.AddVariable(var, buff);
      } 
    }
  }
//  std::cout << "Error Obj Func: " << mErrorObjectiveFunc << std::endl;

  // I'll need to choose a basic variable.
  
  // Iterate over Terms in Expression, replacing variables which
  // are already basic in the tableau.
  for (auto& var : exprVars) {
    if (mRows.find(var) != mRows.end()) {
      expr->Substitute(var, *mRows[var]);
    }
  }
  if (expr->GetConstant() < 0) {
    *expr *= -1;
  }
  std::cout << "Fixed Formed-expr: " << *expr << std::endl;
  std::cout << "Var Count: " << exprVars.size() << std::endl;
  
  // How to check if variable is in tableau?
  // Well it's either a basic var or a parametric var.  
  // Basic vars can be looked up in O(1).
  // Parametric Vars can be looked up in hashset.
  
  // Find Variable that matches some condition.
  Variable chosenBasicVar;
  for (auto& var : exprVars) {
    if (expr->GetCoefficient(var) < 0 && mRows.find(var) == mRows.end() && mParametric.find(var) == mParametric.end()) {
      chosenBasicVar = var;
      break;
    }
  }
  
  if (chosenBasicVar.GetCode() != Variable::Invalid) {
    double n = 1 / (expr->GetCoefficient(chosenBasicVar) * -1);
    *expr *= n;
    expr->RemoveVariable(chosenBasicVar);
    mRows.insert({std::move(chosenBasicVar), expr});
    std::for_each(exprVars.begin(), exprVars.end(), [&](const Variable& v) {
        if (v != chosenBasicVar) mParametric.insert(v);
    });
    return;
  }
  
  std::cout << "Adding Artificial Variable and Optimizing!!" << std::endl;
  
  // Else Add Artificial Variable as BV 
  char varName[10];
  snprintf(varName, sizeof(varName), "a%d", ++mAddedArtificialVarCount);
  Variable artificial(varName, VariableType::Artificial);
  mRows.insert({artificial, expr});
  
  // Setup Objective Function, and solve!
  mObjectiveFunction = *expr; // mObjectiveFunction is a copy of *expr
  
  // Solve LP!
  Solve(mObjectiveFunction);
  
  // If can't be solved, mark it as unsolvable.
  // else get rid of artificial variables. 

  // XXX: Note, I'm assuming that just checking constant
  //      of objective function is enough. 
  //      Since the objective fuction will be an expression
  //      composed of parametric variables. 
  if (!ApproxEq(mObjectiveFunction.GetConstant(), 0.0)) {
    throw std::runtime_error("Can't add Constraint, System not Solvable.");
  }

  // If A still remains as a Basic Variable swap it out 
  // with a parametric variable on the other side.
  // Since both are 0. Then update other rows.
  if (mRows.find(artificial) != mRows.end()) {
    Expression<double>* aVarRow = mRows[artificial];
    std::vector<Variable> aVars = aVarRow->GetVariables();
    if (!aVars.empty()) {
      Variable& chosenBasicVar = aVars[0];
      Pivot(chosenBasicVar, artificial, mObjectiveFunction); 
    }
  } 
  
  // Drop artificial variable from all and move on.
  for (auto& row : mRows) {
    row.second->RemoveVariable(artificial);
  } 
}

void Tableau2::Solve() {
  if (mSolved) return;
  // This is Phase 2 of Two-Phase Simplex. 
  // We're already at a feasible solution. 
  // The only concern we have at this point
  // is if our problem is unbounded. 
  // Solve() detects for unboundedness and throws.

  // Substitute Basic Variables for Error Objective
  std::vector<Variable> errorVars = mErrorObjectiveFunc.GetVariables();
  for (auto& errorVar : errorVars) {
    if (mRows.find(errorVar) != mRows.end()) {
      mErrorObjectiveFunc.Substitute(errorVar, *mRows[errorVar]);
    }
  }
  Solve(mErrorObjectiveFunc);
  mSolved = true;
}

Expression<double>* Tableau2::FormTableauExpression(const Expression<double>& e1, const Relation rel, const Expression<double>& e2, unsigned int strength) {
     Expression<double>* e = new Expression<double>(e1);
    if (!e) return nullptr;
    *e -= e2; // Expression = 0 or Expression <= 0 or Expression >= 0
    if (rel == Relation::LessThanOrEqualTo) {
      *e *= -1;
    }

  char varName[10];
  if (strength == Tableau2::REQUIRED) {
    if (rel != Relation::EqualTo) {
      // Add Slack Variable
      snprintf(varName, sizeof(varName), "s%d", ++mAddedExpressions);
      Variable slackvar(varName, VariableType::Slack);
      e->AddVariable(slackvar, -1); 
    }
  } else { // Optional
    if (rel == Relation::EqualTo) {

        // TODO:
        // XXX: Ahhh, fuck maybe just move this to outside and 
        //      count for non-error variables...
        // For EditVarInfo
        bool singleVar = e->GetVariableCount() == 1;
        Variable onlyVar;
        if (singleVar) {
          onlyVar = e->GetVariables()[0];
        }
      
      // Add +ErrorVar and -ErrorVar
      snprintf(varName, sizeof(varName), "ep%d", ++mAddedExpressions);
      Variable perrorVar(varName, VariableType::Error);
      e->AddVariable(perrorVar, -1);
      
      snprintf(varName, sizeof(varName), "em%d", mAddedExpressions);
      Variable merrorVar(varName, VariableType::Error);
      e->AddVariable(merrorVar, 1);
      
      // Add EditVarInfo if eligible
      if (singleVar) {
        mEditVarInfoMap[onlyVar].plusErrorVar = perrorVar;
        mEditVarInfoMap[onlyVar].minusErrorVar = merrorVar; 

        // c = 3 vs 3 = c ---> c-3=0 | 3-c=0.
        // -v = -1  --> -v + 1 = 0
        // This logic below makes it work for all 3.
        if (e->GetCoefficient(onlyVar) < 0.0) {
          mEditVarInfoMap[onlyVar].originalValue = e->GetConstant();
        } else {
          mEditVarInfoMap[onlyVar].originalValue = -e->GetConstant();
        }
      }
    } else {
      // Add Slack and Error Var
      snprintf(varName, sizeof(varName), "s%d", ++mAddedExpressions);
      Variable slack(varName, VariableType::Slack);
      e->AddVariable(slack, -1);

      snprintf(varName, sizeof(varName), "em%d", mAddedExpressions);
      Variable merror(varName, VariableType::Error);
      e->AddVariable(merror, 1);
    }
  }
  return e;
}

void Tableau2::Resolve() {
  if (mSolved) {
    return;
  }
  // Dual-Simplex Algorithm. Work from Unfeasible but optimal solution
  // to feasible and optimal.
  std::cout << "Resolve:" << mErrorObjectiveFunc << std::endl;
  while (true) {
    // Find exiting basic variable
    Variable exitingVar;
    for (const auto& row : mRows) {
      if (row.second->GetConstant() < 0.0) {
        exitingVar = row.first;
        break;
      }
    }

    if (exitingVar.GetCode() == Variable::Invalid) {
      break;
    }
    
    // Find Entering Variable:
    //   - Perform Minimum Ratio Test
    //
    // XXX: What if we've got a zero coeff
    //      and no positives. Would that be entering var?
    SymbolicWeight<REQUIRED> minRatio(std::numeric_limits<double>::max());
    Variable enteringVar;
    
    const Expression<double>* row = mRows[exitingVar];
    for (const auto& var : row->GetVariables()) { 
      std::cout << "Row Var" << var << std::endl;
      if (row->GetCoefficient(var) > 0.0) {
        // Min-Ratio Test 
        auto symbolicCoeff = mErrorObjectiveFunc.GetCoefficient(var); 
        symbolicCoeff *= (1/row->GetCoefficient(var));
        std::cout << "" << var << " = " << symbolicCoeff << std::endl;
        std::cout << "minRatio for " << var << " = " << symbolicCoeff << std::endl;
        if (symbolicCoeff < minRatio) {
          minRatio = symbolicCoeff;
          enteringVar = var;
        }
      }
    }
    
    if (enteringVar.GetCode() == Variable::Invalid) {
      std::cout << "Only Found Exit Var: " << exitingVar << std::endl;
      throw std::runtime_error("Unsolvable Tableau");
    }
    
    std::cout << "Entering Var: " << enteringVar << std::endl;
    std::cout << "Exiting Var: " << exitingVar << std::endl;
    Pivot(enteringVar, exitingVar, mErrorObjectiveFunc);
  }
  mSolved = true;
}

