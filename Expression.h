#pragma once
#include <array>
#include <cassert>
#include <iostream>
#include <initializer_list>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <vector>
#include <limits>
#include <algorithm>

// Future TODOs
//  2. Change Variable::name type to std::string_view.
//      - And make class-static map to manage lifetime of string allocation.
//  5. More Tests if Time!
//
//  Tuesday: Integration into Rest of Framework

static bool ApproxEq(const double a, const double b);

class Box;

enum class BoxAttribute {
  Left,
  Right,
  Top,
  Bottom,
  Width,
  Height,
  // Add CenterX, Add CenterY Attributes
  NoAttribute
};

enum class Relation {
  LessThanOrEqualTo, 
  GreaterThanOrEqualTo,
  EqualTo
};

enum class VariableType {
  Normal,
  Slack, // Error Variables are categorized as Slack Variables
  Error,
  Artificial, 
};

enum ConstraintStrength {
  REQUIRED=5,
  STRONG=3,
  WEAK=2
};

template<typename CoefficientType>
class Expression;
template<size_t N>
class SymbolicWeight;

// Variable.
// Two Variables are the same by Value-Equality. 
// Hence, we don't maintain a single variable object. there 
// are multiple and the mCode is a sentinel to represent a 
// unique variable.
class Variable {
 public:
  // Constants
  static constexpr int Invalid = -1;

  // Constructor
  Variable() : mCode(Invalid), mType(VariableType::Normal){};
  Variable(const Variable& v) = default;
  Variable(const char* name, VariableType type = VariableType::Normal, int code = GenerateCode()) : mCode(code), mType(type), mName(name) {}
  Variable(std::string_view name, VariableType type = VariableType::Normal, int code = GenerateCode()) : mCode(code), mType(type), mName(name) {}

  // Destructor 
  ~Variable() = default;

  // Assignment Operator
  Variable& operator=(const Variable& v) = default;

  static int GenerateCode() {
    static int globalcode = 0;
    globalcode++;
    return globalcode;
  }

  bool operator==(const Variable& v) const {
    return mCode == v.mCode && mType == v.mType && mName == v.mName;
  }

  bool operator!=(const Variable& v) const {
    return !(*this == v);
  }
  
  // X*5
  // Also need 5*x!!!
  Expression<double> operator*(double c) const;
  //friend Expression<double> operator*(double c, const Variable& v);
  //X + Y
  Expression<double> operator+(const Variable& v) const;
  
  int GetCode() const { return mCode; }
  VariableType GetType() const { return mType; }
  std::string GetName() const { return std::string(mName.cbegin(), mName.cend()); }
 private:
  //static std::unordered_map<std::string, int> VariableMap; TODO Note: If we did this, we'd have to acquire a l lock each time we constructed a Variable. Not Ideal. Unless we only wanted to create Variables from a single-thread.

  int mCode; // Every unique variable is represented with a unique integer value.
  VariableType mType;  
  std::string mName;
};

// Class Template Specialization of struct hash<T> for "Variable" type.
namespace std {
template<>
struct hash<Variable> {
  std::size_t operator()(const Variable& v) const {
    return std::hash<int>{}(v.GetCode());
  }
};
}

template<size_t Coefficients=5>
struct SymbolicWeight {
 static constexpr size_t num_coefficients = Coefficients;
 SymbolicWeight() {
    for (int i=0; i<Coefficients; i++) mCoefficients[i] = 0.0;
 } 

 SymbolicWeight(const double d) {
  for (int i=0; i<Coefficients; i++) mCoefficients[i] = d;
 } 

 template<typename U>
 SymbolicWeight(std::initializer_list<U> init) {
  int idx=0;
  for (auto i=init.begin(); i<init.end(); i++) {
    mCoefficients[idx++] = *i;
  }
  for (; idx<Coefficients; idx++) mCoefficients[idx]=0.0;
 }
  
 template<typename U>
 SymbolicWeight(const std::array<U, num_coefficients>& arr) {
   for (int i=0; i<arr.size(); i++) {
     mCoefficients[i] = arr[i];
   }
 }

 SymbolicWeight(const SymbolicWeight<Coefficients>& weight) : mCoefficients(weight.mCoefficients) {}
  
 SymbolicWeight& operator=(const SymbolicWeight& s) {
   mCoefficients = s.mCoefficients;
   return *this;
 } 
  
 void operator+=(const SymbolicWeight& other) {
   for (int i=0; i<Coefficients; i++) {
     mCoefficients[i] += other.mCoefficients[i];
   }
 }

 void operator-=(const SymbolicWeight& other) {
    for (int i=0; i<Coefficients; i++) {
     mCoefficients[i] -= other.mCoefficients[i];
   }
 }

 void operator*=(const double c) {
   for (int i=0; i<Coefficients; i++) {
     mCoefficients[i] *= c;
   }
 }
 
 SymbolicWeight operator+(const SymbolicWeight& weights) const {
    SymbolicWeight ret(*this);
    ret += weights;
    return ret;
 }

 SymbolicWeight operator-(const SymbolicWeight& weights) const {
   SymbolicWeight ret(*this);
   ret -= weights;
   return ret;
 }

 SymbolicWeight operator*(const double c) const {
  SymbolicWeight ret(*this);
  ret *= c;
  return ret;
 }

 bool operator<(const double c) const {
   SymbolicWeight t(c);
   return *this < t;
 }

 bool operator<(const SymbolicWeight& s) const { // compare lexicographically.
   for (int i=0; i<Coefficients; i++) {
     if (mCoefficients[i] < s.mCoefficients[i]) return true;
     else if (mCoefficients[i] > s.mCoefficients[i]) return false;
   }  
   return false;
 }

 bool operator==(const SymbolicWeight& s) const {
  for (int i=0; i<Coefficients; i++) {
    if (!ApproxEq(mCoefficients[i], s.mCoefficients[i])) return false;
  } 
  return true;
 }

 bool operator!=(const SymbolicWeight& s) const {
   return !(*this == s);
 }

 SymbolicWeight& abs() {
   for (int i=0; i<Coefficients; i++) mCoefficients[i] = std::abs(mCoefficients[i]);
   return *this;
 }  
 
  template<size_t N>  
  friend std::ostream& operator<<(std::ostream& os, const SymbolicWeight<N>& array);

  std::array<double, Coefficients> mCoefficients;
};

template<size_t Coefficients>
std::ostream& operator<<(std::ostream& os, const SymbolicWeight<Coefficients>& weights) {
  os << "[";
  for (int i=0; i<weights.mCoefficients.size(); i++) {
    os << weights.mCoefficients[i];
    if (i < weights.mCoefficients.size()-1) os << ",";
  }
  os << "]";
  return os;
}

template<typename CoefficientType>
class Expression {
 public:
  using coefficient_type = CoefficientType; 

  Expression() : mConstant(0.0) {}
  Expression(double c) : mConstant(c) {}
  Expression(const Variable& v) : mConstant(0.0) {
    mTerms[v] += CoefficientType(1.0);
  }

  Expression(Expression&& e) {
    mTerms = std::move(e.mTerms);
  }
  
  // = default should work yea?
  Expression(const Expression& e) : mTerms(e.mTerms), mConstant(e.mConstant) {
  }

  // Destructor
  ~Expression() = default;

  // Operators
  Expression& operator=(const Expression& e) = default;

  bool operator==(const Expression& other) const {
    if (!ApproxEq(other.mConstant, mConstant) || other.mTerms.size() != mTerms.size());
    for (auto iter = mTerms.cbegin(); iter != mTerms.cend(); iter++) {
      if (other.mTerms.find(iter->first) == other.mTerms.cend()) return false;
      else if (!ApproxEq(other.mTerms.at(iter->first), iter->second)) return false;
    }
    return true;
  }

  bool operator!=(const Expression& other) const {
    return !(*this == other);
  }

  Expression operator+(const double d) const {
    Expression e(*this);
    e.mConstant += d;
    return e;
  }

  Expression operator+(const Variable& v) const {
    Expression e(*this);
    e.mTerms[v] += CoefficientType(1.0);
    e.ClearZeros(v);
    return e; 
  }

  Expression operator+(const Expression& other) const {
    Expression e(*this);
    for (const auto& term : other.mTerms) {
      e.mTerms[term.first] += term.second;
      e.ClearZeros(term.first);
    }  
    e.mConstant += other.mConstant;
    return e;
  }

  void operator+=(const double c) {
    mConstant += c;
  }

  void operator+=(const Variable& v) {
    mTerms[v] += CoefficientType(1.0);
    ClearZeros(v);
  }

  void operator+=(const Expression& other) {
    for (const auto& term : other.mTerms) {
      mTerms[term.first] += term.second;   
      ClearZeros(term.first);
    }
    mConstant += other.mConstant;
  }

  // 5x+1A * 3 ->  
  Expression operator*(double c) const {
    Expression e(*this);
    for (auto& pair : e.mTerms) {
      pair.second *= c;
      e.ClearZeros(pair.first);
    }
    e.mConstant *= c;
    return e;
  }
  
  // XXX: This is something that should maybe be specialized.
  //      Slighly problematic when trying on Expression<SymbolicWeight> class object.
  template<size_t N>
  Expression<SymbolicWeight<N>> operator*(const SymbolicWeight<N>& c) const {
    Expression<SymbolicWeight<N>> ret;
    for (const auto& term : mTerms) {
      ret.AddVariable(term.first, c * term.second);
    }
    ret.AddConstant(c * mConstant);
    return ret; 
  }

//  friend Expression operator*(double c, const Expression<>& e);

  void operator*=(double c) {
    mConstant *= c;
    for (auto& term : mTerms) {
      term.second *= c;
      ClearZeros(term.first);
    }
  }

  Expression operator-() {
    return *this * -1;
  }
  
  Expression operator-(const Variable& v) const {
    return *this + (v*1.0);
  }

  Expression operator-(const Expression& e) const {
    return *this + (e*-1);
  }

  void operator-=(const Variable& v) {
    mTerms[v] -= CoefficientType(1.0);
    ClearZeros(v);
  }
  
  void operator-=(const Expression& e) {
    mConstant -= e.mConstant;
    for (const auto& term : e.mTerms) {
      mTerms[term.first] -= term.second;
      ClearZeros(term.first);
    }
  }

//  // Iterating over elements. 
//  // Is there a better way to do or design this?
//  // I feel like there should be...
//  std::unordered_map<Variable, double>::const_iterator begin() {
//    return mTerms.cbegin();
//  }
//
//  std::unordered_map<Variable, double>::const_iterator end() {
//    return mTerms.cend();
//  }

  // Other
  // XXX: TODO: Does this work?
  template<typename U=CoefficientType, typename std::enable_if_t<!std::is_same_v<U, double>>>
  void AddVariable(const Variable& var, double coeff) {
    mTerms[var] += coeff;
    ClearZeros(var);
  }
  
  void AddVariable(const Variable& var, const CoefficientType& coeff) {
    mTerms[var] += coeff;
    ClearZeros(var);
  }
  
  void RemoveVariable(const Variable& var) {
    mTerms.erase(var);
  }
  
  std::string GetRep() const {
    std::stringstream stream;
    int i=0;
    for (const auto& term : mTerms) {
      stream << term.second << term.first.GetName();
      if (i < mTerms.size() - 1) stream << " + ";
      i++;
    }
    if (mTerms.size() > 0) stream << " + ";
    stream << mConstant;
    return stream.str();;  
  } 

  CoefficientType GetConstant() const {
    return mConstant;
  }

  Expression& Substitute(const Variable& var, const Expression<double>& expr) {
    if (!ContainsVar(var)) return *this;
    CoefficientType coeff = mTerms[var];
    Expression e = expr * coeff; // [3.0, 5.0] * Expression<float>(2.0xVar + 1.5yVar);
                                  // Should produce a new Expression<CoefficientType>
    RemoveVariable(var); 
    *this += e;
    return *this;
  }

  bool ContainsVar(const Variable& v) const {
    return mTerms.count(v) > 0;
  }

  CoefficientType GetCoefficient(const Variable& v) const {
    auto iter = mTerms.find(v);
    return iter == mTerms.end() ? 0.0 : iter->second;
  }
  
  // XXX: Make this only available for test or something?
  std::vector<Variable> GetSlackVariables() const {
    std::vector<Variable> slackVars;
    for (const auto& term : mTerms) {
      if (term.first.GetType() == VariableType::Slack || term.first.GetType() == VariableType::Error) slackVars.push_back(term.first);
    }
    return slackVars;
  }

  std::vector<Variable> GetVariables() const {
    std::vector<Variable> vars;
    for (const auto& term : mTerms) vars.push_back(term.first);
    return vars;
  }

  void AddConstant(const CoefficientType& c) {
    mConstant += c;
  }

  size_t GetVariableCount() const {
    return mTerms.size();
  }

  void Reset() {
    mTerms.clear();
    mConstant = 0.0;
  }

  // Does this need to be a friend of all templates? Yep. For some reason, Maybe ADL?
  template<typename T>
  friend std::ostream& operator<<(std::ostream& os, const Expression<T>& e);
  //friend std::ostream& operator<<(std::ostream& os, const Expression<CoefficientType>& e);

 private:
  void ClearZeros(const Variable& v) {
    if (ApproxEq(mTerms[v], CoefficientType(0.0))) {
      mTerms.erase(v); 
    }
  }

   std::unordered_map<Variable, CoefficientType> mTerms;
   CoefficientType mConstant;
};

//template<size_t N>
//Expression<SymbolicWeight<N>> operator*(const Expression<float>& e, const SymbolicWeight<N>& s) {
//  Expression<SymbolicWeight<N>> ret;
//  return ret;
//}

static std::ostream& operator<<(std::ostream& os, const Variable& v) {
  os << '[' << v.GetName() << '|' << v.GetCode() << '|';
  if (v.GetCode() >= 0) {
    switch (v.GetType()) {
      case VariableType::Normal:
        os << "Normal";
        break;
      case VariableType::Error:
        os << "Error";
        break;
      case VariableType::Slack:
        os << "Slack";
        break;
      case VariableType::Artificial:
        os << "Artificial";
        break;
      default:
        os << "?";
    }
  }
  os << ']';
  return os;
}

template<typename T>
std::ostream& operator<<(std::ostream& os, const Expression<T>& e) {
  os << e.GetRep();
  return os;
}

static Expression<double> operator*(double c, const Variable& v) {
  return v * c;
}

static Expression<double> operator*(double c, const Expression<double>& e) {
  return e * c;
}

static Expression<double> operator-(const Variable& v1, const Variable& v2) {
  Expression<double> e(v1);
  e -= v2;
  return e;
}

// XXX: 
// - We're essentially limiting ourselves to two variables, since we're specifying
// Box and its' attribute --> Which gives a single variable.
//
// BoxOne.Attribute <Relation> m*BoxTwo.attribute2 + constant <--- No need
class Constraint {
 public:
  Constraint(Box* one, BoxAttribute attrOne, Relation r, Box* two, BoxAttribute attrTwo, double m, double c, int strength = REQUIRED) : mBoxLeft(one), mBoxRight(two), mAttrLeft(attrOne), mAttrRight(attrTwo), 
      mRelation(r), mMultiplier(m), mConstant(c), mStrength(strength) {
//    assert(mStrength > 0 && mStrength < REQUIRED && "Constraint Strength is not in range [0, 1000]");
  }

  Variable GetVarOne() const {
    return GetVar(mBoxLeft, mAttrLeft); 
  }

  Variable GetVarTwo() const {
    return GetVar(mBoxRight, mAttrRight); 
  }

  double GetMultiplier() const {
    return mMultiplier;
  }

  Relation GetRelation() const {
    return mRelation;
  }

  double GetConstant() const {
    return mConstant;
  }

  int GetStrength() const {
    return mStrength;
  }

  void UpdateConstant(double c) {
    mConstant = c;
  }

 private:
   static Variable GetVar(const Box* const b, BoxAttribute attribute);

  Box* const mBoxLeft;
  Box* const mBoxRight;
  BoxAttribute mAttrLeft;
  BoxAttribute mAttrRight;
  Relation mRelation;
  double mMultiplier;
  double mConstant;
  int mStrength;
};

// How do I choose Basic Variable?
// That's part of Phase 1, yea?

//
// Optimization Part:
// === Iterate through Objective Function ===
// When we encounter a negative coefficient,
// === look at all Rows, which have that variable. ===
// Find the one which passes the minimum-ratio-test.
// Now we have an entering and exit variable.
// Pivot!!!
// Pivoting is easy with a tableau.
// Iterate through all rows

// Ok, so how do I represent constraints?
// How do I represent Variables?
class Tableau2 {
  private:
   struct EditVarInfo {
     Variable plusErrorVar;
     Variable minusErrorVar;
     double originalValue;
   }; 

  public:
   void AddConstraint(Constraint* const c) {
      assert(c && "Tableau: Can't Add nullptr Constraint");
      Expression<double> eOne = c->GetVarOne();
      if (c->GetVarOne().GetCode() == Variable::Invalid) {
        throw std::runtime_error("Constraint Must have Left-Side Attribute");
      }
      Expression<double> eTwo(c->GetConstant());
      if (c->GetVarTwo().GetCode() != Variable::Invalid) {
        eTwo.AddVariable(c->GetVarTwo(), c->GetMultiplier());
      }
      AddConstraint(eOne, c->GetRelation(), eTwo, c->GetStrength());
   }

   void AddConstraint(const Expression<double>& e1, const Relation r, const Expression<double>& e2, unsigned int strength=REQUIRED);
   Expression<double>* FormTableauExpression(const Expression<double>& e1, const Relation r, const Expression<double>& e2, unsigned int strength=REQUIRED);

   bool ContainsVar(const Variable& var) {
     return (mParametric.find(var) != mParametric.end()) || (mRows.find(var) != mRows.end());
   }
    
   // EditVar's are of the form, editVar 
   void UpdateConstraint(const Variable& editVar, const double newValue) {
     mSolved = false;
    // XXX: Note, We do NOT update constant term in error objective function.
    //      It seems to be easily do-able though:
    //      when both are parametric: While we go through rows to update, if 
    //      row has basic variable that is error variable, 
    //      then take it's newly added-to constant and add it also to the objective fucntion!!!
    //      Why? Rememeber error objective function represents:
    //        [1,0]e1 + [1,0]e2 + [0,1]e3 + [0,1]e4 ...
    //      TODO: Can't do this until I store the original symbolic weight 
    //      of each error variable. Which could easily be done with
    //      std::unordered_map<Variable, int> strengthMap;
    auto iter = mEditVarInfoMap.find(editVar);
    if (iter == mEditVarInfoMap.end()) {
      throw std::runtime_error("Can't modify non-edit var");
    }
    EditVarInfo& editInfo = iter->second;
    const double difference = newValue - editInfo.originalValue;

    if (mRows.find(editInfo.plusErrorVar) != mRows.end()) {
      *mRows[editInfo.plusErrorVar] += -difference;
    } else if (mRows.find(editInfo.minusErrorVar) != mRows.end()) {
      *mRows[editInfo.minusErrorVar] += difference;
    } else {
      assert(mParametric.find(editInfo.plusErrorVar) != mParametric.end() && "Plus Error Var is Parametric but not located in Parametric Set!");
      assert(mParametric.find(editInfo.minusErrorVar) != mParametric.end() && "Minus Error Var is Parametric but not location in Tableau's Parametric Set!");

      for (auto& row : mRows) {
         Expression<double>* const expr = row.second;
         double coeff = expr->GetCoefficient(editInfo.plusErrorVar);
         *expr += (difference * coeff);

         // XXX: Skip for now until TODO for error variable symbolic weights is 
         //      addressed above.
         // If basic variable is error variable, update constant in error objective.
//         if (row.first.GetType() == VariableType::Error) {
//            decltype(mErrorObjectiveFunc) delta = {??? coeff * difference }
//            mErrorObjectiveFunc += delta;
//         }
      } 

    }
    editInfo.originalValue = newValue;
   }

   void FinishUpdates() {
    bool needsResolve = false;
    for (auto& row : mRows) {
      if (row.second->GetConstant() < 0.0) {
        std::cout << "Need to reoptimize!!!" << std::endl;
        needsResolve = true;
        break;
      }
    }
    if (needsResolve) {
      try {
      Resolve();
      } catch (std::exception& e) {
        std::cout << "Unsolvable Tableau:" << std::endl;
        std::cout << GetRep() << std::endl;
        exit(127);
      }
    }
   }

   void UpdateConstraint(const Constraint& c, const double newConstant) {
    // Box.Attribute = multiplier * RightBox.Attribute + constant
    // In this case, assume it would be of the form
    // Box.Attribute = m * NONE + constant
      UpdateConstraint(c.GetVarOne(), newConstant);
   } 
   
   void Reset() {
     mExternalConstraints.clear();
     mInternalConstraints.clear();
     mRows.clear();
     mParametric.clear();
     mObjectiveFunction.Reset();
     mErrorObjectiveFunc.Reset();
     mEditVarInfoMap.clear();
     mAddedArtificialVarCount = 0;
     mAddedExpressions = 0;
     mSolved = true;
   }
   
   template<typename T>
   void Solve(Expression<T>& objective);
   
   void Solve();
   void Resolve();


   void ProduceSolverConstraints();
   std::string GetRep() const;

   double GetResultOrDefault(const Variable& v, double deflt) {
      if (!mSolved) {
        Solve();
      }
      auto iter = mRows.find(v);
      if (iter != mRows.end()) {
        return iter->second->GetConstant();
      }
      
      auto iterParametric = mParametric.find(v); 
      if (iterParametric != mParametric.end()) {
        return 0.0;
      }

      return deflt;
   }

   double GetResult(const Variable& v) {
    return GetResultOrDefault(v, -1.0);
   }
   
   static constexpr int REQUIRED=5;
   static constexpr int STRONG=4; 
   static constexpr int WEAK=2;

   ~Tableau2() {
     for (auto& p : mRows) {
       delete p.second;
     }
     mRows.clear();
   }
 
 private:
    template<typename T>
    void Pivot(const Variable& enteringVar, const Variable& exitingVar, Expression<T>& mObjectiveFunction);
   
     // Constraints added by the User
     std::vector<Constraint*> mExternalConstraints;

     // Constraints generated by the Tableau/Solver
     std::vector<Constraint*> mInternalConstraints;
      
     std::unordered_map<Variable, Expression<double>*> mRows;
     std::unordered_set<Variable> mParametric;
     Expression<double> mObjectiveFunction;
     
     Expression<SymbolicWeight<REQUIRED>> mErrorObjectiveFunc;

     std::unordered_map<Variable, EditVarInfo> mEditVarInfoMap; // An EditVarInfo 
                                                             // is created for each constraint
                                                             // which is eligible to be one.
     
     int mAddedArtificialVarCount = 0;
     int mAddedExpressions = 0;
     bool mSolved = true;
}; 

template<typename T>
void Tableau2::Solve(Expression<T>& objectiveFunction) {
  std::cout << "Solve: " << objectiveFunction << std::endl;
  std::cout << GetRep() << std::endl;
  while (true) {
    // Find an entry variable.
    Variable enteringVar;
    std::vector<Variable> objVars = objectiveFunction.GetVariables();
    // I really should add a Expression::TermIterator of some sort.  
    for (auto& var : objVars) {
      if (objectiveFunction.GetCoefficient(var) != 0.0 && objectiveFunction.GetCoefficient(var) < 0.0) { 
        // XXX: Make sure coefficient is not ApproxEq(i,0.0)
        //      Since it could happen...
       
        enteringVar = var;
        break;
      }
    }  
    
    if (enteringVar.GetCode() == -1) {
      break;
    }

    // Find exiting var by MRT
    double minRatio = std::numeric_limits<double>::max();
    Variable exitingVar;
    for (auto& pair : mRows) {
      Expression<double>* expr = pair.second;
      // TODO:What was rule for those rows with zero?
      if (expr->GetCoefficient(enteringVar) < 0.0) {
        double ratio = -expr->GetConstant() / expr->GetCoefficient(enteringVar);
        if (ratio < minRatio) {
          minRatio = ratio;
          exitingVar = pair.first;
        }
      }
    }  

    if (exitingVar.GetCode() == Variable::Invalid) { // No viable exiting variable found. 
                                                     // Problem unbounded.
      throw std::runtime_error("Unbounded Problem"); 
    }
    
    std::cout << "Pivot" << "EnteringVar: " << enteringVar.GetName() << std::endl;
    std::cout << "Pivot" << "ExitingVar: " << exitingVar.GetName() << std::endl;
    Pivot(enteringVar, exitingVar, objectiveFunction);
    std::cout << "Pivot Complete" << std::endl;
    std::cout << GetRep() << std::endl;
  }  
}

template<typename T>
void Tableau2::Pivot(const Variable& enteringVar, const Variable& exitingVar, Expression<T>& objective) {
  assert(enteringVar != exitingVar && "Entering Variable and Exiting Variable are the same");
  Expression<double>* row = mRows[exitingVar];
  mRows.erase(exitingVar);
  assert(row != nullptr && "Can't Pivot on null Expression");
  
  double enterCoeff = row->GetCoefficient(enteringVar);
  row->RemoveVariable(enteringVar);
  row->AddVariable(exitingVar, -1);
  *row *= (-1/enterCoeff);
  
  objective.Substitute(enteringVar, *row);
  for (auto& pair : mRows) {
    pair.second->Substitute(enteringVar, *row);
  }
  
  mParametric.erase(enteringVar);
  mParametric.insert(exitingVar);
  mRows.insert({enteringVar, row});
}

static std::ostream& operator<<(std::ostream& os, const Tableau2& t) {
  os << t.GetRep();
  return os;
}
 

static bool ApproxEq(const double a, const double b) {
// Seems like there's a better way to do this:
//stackoverflow.com/questions/35158493/how-to-choose-epsilon-value-for-floating-point
  static constexpr double epsilon = 1e-6;
  return std::abs(a-b) < epsilon; 
}


template<size_t N>
static bool ApproxEq(const SymbolicWeight<N>& a, const SymbolicWeight<N>& b) {
    return a == b;
}

