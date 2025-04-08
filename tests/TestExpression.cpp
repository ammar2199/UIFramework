#include <iostream>
#include "gtest/gtest.h"
#include "../Expression.h"
#include "../View.h"

// TODO: Clean all this up...
//       Move to multiple different
//       files. I just shoved everying
//       in here.
 
TEST(SetupTest, A) {
  EXPECT_EQ(1, 1);
  EXPECT_EQ(3, 3);
}

TEST(ExpressionTest, A) {
  Variable X("X");
  Variable Y("Y");
  Variable Z("Z");

  Expression<double> e = 6*X + Y + 3*Z + 7;
  Expression<double> z = 9*Z + -2 * Y;
  e += z;
  e += Y;
  e += Y;
  e += e;

  e = 3*e;
  e.RemoveVariable(Y);
  
  std::string output = e.GetRep();
  std::cout << output << std::endl;
  EXPECT_GT(output.length(), 0);
}

TEST(Expression, SubstituteTest) {
  Variable X("X");
  Variable Y("Y");
  Variable Z("Z");

  Expression<double> e1 = -2*X + (Y * -1);
  Expression<double> e2 =  -2*Y + Z;
  e2 += 3;
  e1.Substitute(X, e2); // e1 = 3Y - 2z - 6
  std::cout << e1 << std::endl;
}

TEST(ExpressionTest, EqualityTest) {
  Variable varX("x");
  Variable varY("y");
  Variable varZ("z");
  Expression<double> e = varX; 
  Expression<double> e2 = 2.5*varX + 51.6*varY;
  Expression<double> eM = 0.5*varX + 17.2*varY;
  e2 -= eM;
  e2 -= eM;
  e2 -= eM;
  ASSERT_EQ(e, e2);
  Expression<double> e3 = varX + varZ;
  ASSERT_NE(e3, e);  
}

TEST(ExpressionTest, FormTableauExpression) {
  Tableau2 tableau;
  
  Variable varA("A");
  Variable varB("B");
  Expression<double> e1 = varA - varB;
  Expression<double>* result = tableau.FormTableauExpression(e1, Relation::EqualTo, 20.0, Tableau2::STRONG);
  EXPECT_NE(result, nullptr);
  auto slackVars = result->GetSlackVariables();
  Expression<double> expected = varA - varB - 20;
  for (auto& var : slackVars) expected.AddVariable(var, result->GetCoefficient(var)); 
  ASSERT_EQ(*result, expected);

  // TODO: >= and <= and = (required) and etc, etc.
}

TEST(TableauTest, AddConstraint1) {
  // No Artificial Variable needed here.
  Tableau2 tableau;
  Variable varA("A");
  Variable varB("B");
  Expression<double> e1 = varA - varB;
  Expression<double> e2 = varA;
  tableau.AddConstraint(e1, Relation::EqualTo, 20, Tableau2::STRONG);
  tableau.AddConstraint(e2, Relation::LessThanOrEqualTo, 100, Tableau2::REQUIRED);
  tableau.AddConstraint(e2, Relation::EqualTo, 100, Tableau2::REQUIRED); // ALSO WORKS!
  //tableau.AddConstraint(e2, Relation::EqualTo, 90, Tableau2::REQUIRED); // WORKS! Either, Or.
  std::cout << "Final Tableau" << std::endl;
  std::cout << tableau << std::endl;
  std::cout << "Final Tableau2" << std::endl;
  tableau.Solve();
  std::cout << tableau << std::endl;
}

TEST(SymbolicWeightTest, TestOne) {
  SymbolicWeight<3> s1;
  SymbolicWeight<5> s2;
  std::cout << s1 << std::endl; 
  std::cout << s2 << std::endl;

  SymbolicWeight<5> s3(1);
  SymbolicWeight<5> s4(3);
  std::cout << s3 + s4 << std::endl;
  
  SymbolicWeight<3> A1 = {1, 2, 3};
  SymbolicWeight<3> A2 = {1.1, 2.2, 3.3};
  SymbolicWeight<2> A3 = {-1.3, -15.74};
  std::cout << A3.abs() << std::endl;
  std::cout << A1 << std::endl;
  std::cout << A2 << std::endl;
  std::cout << (A1 < A2) << std::endl;
  std::cout << (A2 < A1) << std::endl;
  ASSERT_LT(A1, A2);
  ASSERT_FALSE(A2 < A1);
  A2 *= 3;
  std::cout << A2 << std::endl;
  A2 += A2;
  std::cout << A2 << std::endl;
  A2 -= (SymbolicWeight<3>({3.3, 6.6, 9.9}) * 2);
  std::cout << A2 << std::endl;
  ASSERT_TRUE(ApproxEq(A2, SymbolicWeight<3>(0.0)));
  ASSERT_EQ(A2, SymbolicWeight<3>(0.0));
}

TEST(ExpressionTest2, SymbolicExpressions) {
  Variable xVar("X");
  Expression<SymbolicWeight<2>> expression(xVar);
  Expression<SymbolicWeight<2>> expression2(xVar);
  //decltype(expression) expr3 = {1, 2}*xVar; <--- Would be cool.
  std::cout << expression << std::endl;
  std::cout << expression2 << std::endl;
  ASSERT_EQ(expression, expression2);
  Expression<SymbolicWeight<2>> expr3 = expression + expression2;
  Expression<SymbolicWeight<2>> expr4;
  expr4.AddVariable(xVar, {2, 2});
  ASSERT_EQ(expr3, expr4);
  
  Expression<SymbolicWeight<2>> expr5;
  expr5.AddVariable(xVar, {1.7, 33.7541});
  expr5.AddVariable(xVar, {-1.7, -33.7541});
  std::cout << expr5 << std::endl;
  ASSERT_EQ(expr5.GetVariables().size(), 0);
  
  expr5 += 1.5;
  expr5 += xVar;
  expr5 -= xVar;
  expr5 += xVar;
  std::cout << expr5 << std::endl;
  
  Variable yVar("Y");
  Variable zVar("Z");
  Expression<double> exprA = yVar;
  Expression<double> exprB = exprA + zVar;
  exprB += zVar;
  exprB += zVar;
  std::cout << exprB << std::endl;

  std::cout << expr5.Substitute(xVar, exprB) << std::endl;

  SymbolicWeight<2> aSym = {1, 2};
  Expression<double> exprC(1.5);
  std::cout << exprC * aSym << std::endl;
  
//  Expression<SymbolicWeight<2>> exprD; XXX: Causes problems. Needs to be fixed.
//  exprD.AddVariable(xVar, {93, 66});
//  std::cout << exprD * aSym << std::endl;
} 

TEST(ExpressionTest, ArrayInit) {
  constexpr size_t NumCoefficients = 3;
  std::array<int, NumCoefficients> arrInt = {1, 2, 3};
  std::array<double, NumCoefficients> arrDouble = {1.5, 2.67, -25.91};
  SymbolicWeight<NumCoefficients> aSym(arrInt);
  SymbolicWeight<NumCoefficients> bSym(arrDouble);
  std::cout << aSym << std::endl;
  std::cout << bSym << std::endl;
}

TEST(TableauTest, ErrorStrengthTest) {
  Tableau2 tableau;
  Variable xVar("X");
  Expression<double> a = 2*xVar + 3;
  tableau.AddConstraint(a, Relation::EqualTo, 20, Tableau2::WEAK); 
  tableau.AddConstraint(a, Relation::LessThanOrEqualTo, 20, 3); 
  // XXX: Alright, this seems to be functioning properly continue on with AddConstraint
  //      and Solve()
}

TEST(TableauTest, Problem1) {
  Variable windowLeft("WindowLeft");
  Variable windowRight("WindowRight");
  Variable windowWidth("WindowWidth");
  Variable boxLeft("BoxLeft"), boxRight("BoxRight"), boxWidth("BoxWidth");
  
  Tableau2 tableau;
    
  // Framework-Generated Constraints
  constexpr int WindowWidth = 240;
  tableau.AddConstraint(windowLeft, Relation::EqualTo, 0, Tableau2::REQUIRED);
  tableau.AddConstraint(windowRight, Relation::EqualTo, WindowWidth, Tableau2::REQUIRED);
  tableau.AddConstraint(windowWidth, Relation::EqualTo, WindowWidth, Tableau2::REQUIRED);
  // XXX: Is this irrelevant?
  //      Maybe add a check to just not add row if expr.GetVariables().size == 0
//  tableau.AddConstraint(windowRight- windowLeft, Relation::EqualTo, windowWidth, Tableau2::REQUIRED);
  tableau.AddConstraint(boxRight-boxLeft, Relation::EqualTo, boxWidth, Tableau2::REQUIRED);

  //App-Provided Constraints
  tableau.AddConstraint(boxWidth, Relation::GreaterThanOrEqualTo, 40, Tableau2::STRONG);
  tableau.AddConstraint(boxWidth, Relation::LessThanOrEqualTo, 100, Tableau2::STRONG);
  tableau.AddConstraint(boxWidth, Relation::EqualTo, windowWidth * (1.0/3), Tableau2::WEAK);
  tableau.AddConstraint(boxLeft, Relation::EqualTo, windowLeft, Tableau2::STRONG);
  
  std::cout << "Solving....." << std::endl; 
  tableau.Solve();
  std::cout << tableau << std::endl;
}

TEST(TableauTest, Problem2) {
  Variable aVar("A");
  Variable bVar("B");
  Expression<double> expr1 = aVar - bVar;
  
  Tableau2 tableau;
  tableau.AddConstraint(expr1, Relation::EqualTo, 20, Tableau2::STRONG);
  tableau.AddConstraint(aVar, Relation::LessThanOrEqualTo, 100, Tableau2::REQUIRED);
  tableau.AddConstraint(aVar, Relation::EqualTo, 100, Tableau2::REQUIRED);
  
  std::cout << tableau << std::endl;
}

TEST(SymbolicVariable, SymbolicInit) {
  Variable aVar("A");
  //auto e = aVar * {1, 2}; //<--- Can't easiy be done. We can't infer the type of the operator*.
  //Expression<SymbolicWeight<5>> e2 = Variable::operator*<int, 5>({1, 2}, &aVar);
  //std::cout << e << std::endl;
}

TEST(TableauTest, ConstraintConversion) {
  Box a; 
  Box b;
  std::cout << a.GetLeftVar() << std::endl;
  std::cout << a.GetRightVar() << std::endl;
  std::cout << a.GetTopVar() << std::endl;
  std::cout << a.GetBottomVar() << std::endl;

  Constraint* c = new Constraint(&a, BoxAttribute::Left, Relation::EqualTo, &b, BoxAttribute::Left, 1.0, -20.0);

  Tableau2 tableau;
  tableau.AddConstraint(c);
  std::cout << tableau << std::endl;
}

TEST(TableauTest, EditVarTest) {
  Tableau2 tableau;
  Variable aVar = ("aVar");
  tableau.AddConstraint(aVar, Relation::EqualTo, 20, Tableau2::STRONG);
  std::cout << tableau << std::endl;
  std::cout << "==== Updating ====\n";
  tableau.UpdateConstraint(aVar, 44);
  std::cout << tableau << std::endl;
}

TEST(TableauTest, UpdateConstraint) {
  Variable windowLeft("WindowLeft");
  Variable windowRight("WindowRight");
  Variable windowWidth("WindowWidth");
  Variable boxLeft("BoxLeft"), boxRight("BoxRight"), boxWidth("BoxWidth");
  
  Tableau2 tableau;
    
  // Framework-Generated Constraints
  constexpr int WindowWidth = 150;
  tableau.AddConstraint(windowLeft, Relation::EqualTo, 0, Tableau2::STRONG);
  tableau.AddConstraint(windowRight, Relation::EqualTo, WindowWidth, Tableau2::STRONG);
  tableau.AddConstraint(windowWidth, Relation::EqualTo, WindowWidth, Tableau2::STRONG);
  // XXX: Is this irrelevant?
  //      Maybe add a check to just not add row if expr.GetVariables().size == 0
//  tableau.AddConstraint(windowRight- windowLeft, Relation::EqualTo, windowWidth, Tableau2::REQUIRED);
  tableau.AddConstraint(boxRight-boxLeft, Relation::EqualTo, boxWidth, Tableau2::STRONG);

  //App-Provided Constraints
  constexpr int STRONG = 3;
  tableau.AddConstraint(boxWidth, Relation::GreaterThanOrEqualTo, 40, STRONG);
  tableau.AddConstraint(boxWidth, Relation::LessThanOrEqualTo, 100, STRONG);
  tableau.AddConstraint(boxWidth, Relation::EqualTo, windowWidth * (1.0/3), Tableau2::WEAK);
  tableau.AddConstraint(boxLeft, Relation::EqualTo, windowLeft, Tableau2::STRONG);
  
  std::cout << "Solving....." << std::endl; 
  tableau.Solve();
  std::cout << tableau << std::endl;
  
  // So this is an example that needs to be re-optimized. 
  // The tableau, update seems to be working.
  std::cout << "===UPDATING===" << std::endl;
  tableau.UpdateConstraint(windowWidth, 400);
  tableau.UpdateConstraint(windowRight, 400); // XXX: Also, try to remove this. Then the re-optimization
                                              //      should fail bc then the problem becomes infeasible?
                                              //      Well actually, maybe no. Hmmm
                                              //      maybe the box-constraints generated by the 
                                              //      system should be Tableau2::Required!
                                              //      Yea, that seems to make sense.
                                              //      Only the Window edge constraints need to be changed.
  tableau.FinishUpdates();
  std::cout << "=== DONE UPDATING ===" << std::endl;
  std::cout << tableau << std::endl; // Sweet, this works.
                                     //
  
  std::cout << "=== ANOTHER UPDATE ===" << std::endl;
  tableau.UpdateConstraint(windowWidth, 70);
  tableau.UpdateConstraint(windowRight, 70);
  tableau.FinishUpdates();  // Nice, this works too.
  std::cout << "=== DONE WITH 3RD UPDATE ===" << std::endl;
  std::cout << tableau << std::endl;
}

TEST(TableauTest, GeneratedConstraint) {
  // Imagine a situation where we've added a 
  // Right Constraint for Box 2, have added all BoxXConstraints System-Generated Constraints.
  // but have not added a Width or Left Constraint.
  // Is it possible to know this?
  // Yes, it seems so. 
  Variable Box1Left("Box1Left"), Box1Right("Box1Right"), Box1Width("Box1Width");
  Variable Box2Left("Box2Left"), Box2Right("Box2Right"), Box2Width("Box2Width");
  Variable Box3Left("Box3Left"), Box3Right("Box3Right"), Box3Width("Box3Width");
  
  Tableau2 tableau;
  tableau.AddConstraint(Box1Left, Relation::EqualTo, Box2Right);
  tableau.AddConstraint(Box2Right, Relation::EqualTo, Box3Right);
  tableau.AddConstraint(Box3Right, Relation::EqualTo, 100);

  // System Box Constraints
  tableau.AddConstraint(Box1Width, Relation::EqualTo, Box1Right - Box1Left);
  tableau.AddConstraint(Box2Width, Relation::EqualTo, Box2Right - Box2Left);
  tableau.AddConstraint(Box3Width, Relation::EqualTo, Box3Right - Box3Left);

  tableau.Solve();
  std::cout << "=== Solution ===" << std::endl;
  std::cout << tableau << std::endl;

  tableau.AddConstraint(Box2Left, Relation::EqualTo, 10);
  std::cout << "=== Solution2 ===" << std::endl;
  std::cout << tableau << std::endl;
}


