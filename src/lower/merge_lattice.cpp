#include "merge_lattice.h"

#include <set>
#include <algorithm>

#include "taco/index_notation/index_notation.h"
#include "taco/index_notation/index_notation_nodes.h"
#include "taco/index_notation/index_notation_visitor.h"
#include "tensor_path.h"
#include "iteration_graph.h"
#include "iterators.h"
#include "mode_access.h"
#include "taco/util/collections.h"
#include "taco/util/strings.h"

using namespace std;

namespace taco {

// class MergeLattice
MergeLattice::MergeLattice() {
}

MergeLattice::MergeLattice(std::vector<MergeLatticePoint> points,
                           std::vector<Iterator> resultIterators)
    : points(points), resultIterators(resultIterators) {
}

template <class op>
static
MergeLattice scale(MergeLattice lattice, IndexExpr scale, bool leftScale) {
  std::vector<MergeLatticePoint> scaledPoints;
  for (auto& point : lattice) {
    IndexExpr expr = point.getExpr();
    IndexExpr scaledExpr = (leftScale) ? new op(scale, expr)
                                       : new op(expr, scale);
    MergeLatticePoint scaledPoint(point.getIterators(),
                                  point.getMergeIterators(),
                                  point.getRangeIterators(),
                                  scaledExpr);
    scaledPoints.push_back(scaledPoint);
  }
  return MergeLattice(scaledPoints, lattice.getRangeIterators());
}

template <class op>
static MergeLattice scale(IndexExpr expr, MergeLattice lattice) {
  return scale<op>(lattice, expr, true);
}

template <class op>
static MergeLattice scale(MergeLattice lattice, IndexExpr expr) {
  return scale<op>(lattice, expr, false);
}

template <class op>
static MergeLattice unary(MergeLattice lattice) {
  std::vector<MergeLatticePoint> negPoints;
  for (auto& point : lattice) {
    IndexExpr negExpr = new op(point.getExpr());
    negPoints.push_back(MergeLatticePoint(point.getIterators(),
                                          point.getMergeIterators(),
                                          point.getRangeIterators(),
                                          negExpr));
  }
  return MergeLattice(negPoints, lattice.getRangeIterators());
}

MergeLattice MergeLattice::make(Forall forall,
                                const map<ModeAccess,Iterator>& iterators) {
  struct MakeMergeLattice : public IndexNotationVisitorStrict {
    IndexVar i;
    map<ModeAccess,Iterator> iterators;
    MergeLattice lattice;

    MergeLattice makeLattice(IndexStmt stmt) {
      stmt.accept(this);
      MergeLattice l = lattice;
      lattice = MergeLattice();
      return l;
    }

    MergeLattice makeLattice(IndexExpr expr) {
      expr.accept(this);
      MergeLattice l = lattice;
      lattice = MergeLattice();
      return l;
    }

    Iterator getIterator(Access access) {
      size_t loc = util::locate(access.getIndexVars(),i) + 1;
      taco_iassert(util::contains(iterators, ModeAccess(access,loc)));
      return iterators.at(ModeAccess(access,loc));
    }

    void visit(const AccessNode* access) {
      Iterator iterator = getIterator(access);
      MergeLatticePoint latticePoint =
          MergeLatticePoint({iterator}, {iterator}, {iterator}, access);
      lattice = MergeLattice({latticePoint}, {});
    }

    void visit(const LiteralNode* node) {
      taco_not_supported_yet;
    }

    void visit(const NegNode* node) {
      lattice = makeLattice(node->a);
    }

    void visit(const AddNode* node) {
      taco_not_supported_yet;
    }

    void visit(const SubNode* node) {
      taco_not_supported_yet;
    }

    void visit(const MulNode* node) {
      taco_not_supported_yet;
    }

    void visit(const DivNode* node) {
      taco_not_supported_yet;
    }

    void visit(const SqrtNode* node) {
      taco_not_supported_yet;
    }

    void visit(const ReductionNode* node) {
      taco_not_supported_yet;
    }

    void visit(const AssignmentNode* node) {
      MergeLattice l = makeLattice(node->rhs);
      lattice = MergeLattice(l.getMergePoints(), {getIterator(node->lhs)});
    }

    void visit(const ForallNode* node) {
      taco_not_supported_yet;
    }

    void visit(const WhereNode* node) {
      taco_not_supported_yet;
    }

    void visit(const MultiNode* node) {
      taco_not_supported_yet;
    }

    void visit(const SequenceNode* node) {
      taco_not_supported_yet;
    }
  };

  MakeMergeLattice make;
  make.i = forall.getIndexVar();
  make.iterators = iterators;
  return make.makeLattice(forall.getStmt());
}

MergeLattice MergeLattice::make(const IndexExpr& indexExpr,
                                const IndexVar& indexVar,
                                const old::IterationGraph& iterationGraph,
                                const old::Iterators& iterators) {

  struct BuildMergeLattice : public IndexExprVisitorStrict {
    const IndexVar&            indexVar;
    const old::IterationGraph& iterationGraph;
    const old::Iterators&      iterators;
    MergeLattice               lattice;

    BuildMergeLattice(const IndexVar& indexVar,
                      const old::IterationGraph& iterationGraph,
                      const old::Iterators& iterators)
        : indexVar(indexVar), iterationGraph(iterationGraph),
          iterators(iterators) {
    }

    MergeLattice buildLattice(const IndexExpr& expr) {
      expr.accept(this);
      MergeLattice l = lattice;
      lattice = MergeLattice();
      return l;
    }

    using IndexExprVisitorStrict::visit;

    void visit(const AccessNode* expr) {
      // Throw away expressions `indexVar` does not contribute to
      if (!util::contains(expr->indexVars, indexVar)) {
        lattice = MergeLattice();
        return;
      }

      old::TensorPath path = iterationGraph.getTensorPath(expr);
      size_t i = util::locate(path.getVariables(), indexVar);
      Iterator iter = iterators[path.getStep(i)];
      auto latticePoint = MergeLatticePoint({iter}, {iter}, {iter}, expr);
      lattice = MergeLattice({latticePoint}, {});
    }

    void visit(const LiteralNode*) {
    }

    void visit(const NegNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      lattice = unary<NegNode>(a);
    }

    void visit(const SqrtNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      lattice = unary<SqrtNode>(a);
    }

    void visit(const AddNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      MergeLattice b = buildLattice(expr->b);
      if (a.defined() && b.defined()) {
        lattice = disjunction<AddNode>(a, b);
      }
      // Scalar operands
      else if (a.defined()) {
        lattice = scale<AddNode>(a, expr->b);
      }
      else if (b.defined()) {
        lattice = scale<AddNode>(expr->a, b);
      }
    }

    void visit(const SubNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      MergeLattice b = buildLattice(expr->b);
      if (a.defined() && b.defined()) {
        lattice = disjunction<SubNode>(a, b);
      }
      // Scalar operands
      else if (a.defined()) {
        lattice = scale<SubNode>(a, expr->b);
      }
      else if (b.defined()) {
        lattice = scale<SubNode>(expr->a, b);
      }
    }

    void visit(const MulNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      MergeLattice b = buildLattice(expr->b);
      if (a.defined() && b.defined()) {
        lattice = conjunction<MulNode>(a, b);
      }
      // Scalar operands
      else if (a.defined()) {
        lattice = scale<MulNode>(a, expr->b);
      }
      else if (b.defined()) {
        lattice = scale<MulNode>(expr->a, b);
      }
    }

    void visit(const DivNode* expr) {
      MergeLattice a = buildLattice(expr->a);
      MergeLattice b = buildLattice(expr->b);
      if (a.defined() && b.defined()) {
        lattice = conjunction<DivNode>(a, b);
      }
      // Scalar operands
      else if (a.defined()) {
        lattice = scale<DivNode>(a, expr->b);
      }
      else if (b.defined()) {
        lattice = scale<DivNode>(expr->a, b);
      }
    }

    void visit(const ReductionNode* expr) {
      lattice = buildLattice(expr->a);
    }
  };

  auto lattice = BuildMergeLattice(indexVar, iterationGraph,
                                   iterators).buildLattice(indexExpr);
  taco_iassert(lattice.getSize() > 0) <<
      "Every merge lattice should have at least one lattice point";
  return lattice;
}

size_t MergeLattice::getSize() const {
  return points.size();
}

const MergeLatticePoint& MergeLattice::operator[](size_t i) const {
  return points[i];
}

const std::vector<MergeLatticePoint>& MergeLattice::getMergePoints() const {
  return points;
}

const std::vector<Iterator>& MergeLattice::getMergeIterators() const {
  // The iterators merged by a lattice are those merged by the first point
  taco_iassert(points.size() > 0) << "No lattice points in the merge lattice";
  return points[0].getIterators();
}

const std::vector<Iterator>& MergeLattice::getRangeIterators() const {
  // The iterators merged by a lattice are those merged by the first point
  taco_iassert(points.size() > 0) << "No lattice points in the merge lattice";
  return points[0].getRangeIterators();
}

const std::vector<Iterator>& MergeLattice::getResultIterators() const {
  return resultIterators;
}

const IndexExpr& MergeLattice::getExpr() const {
  taco_iassert(points.size() > 0) << "No lattice points in the merge lattice";

  // The expression merged by a lattice is the same as the expression of the
  // first lattice point
  return points[0].getExpr();
}

MergeLattice MergeLattice::getSubLattice(MergeLatticePoint lp) const {
  // A lattice point lp dominats lq iff it contains a subset of lp's
  // tensor path steps. So we scan through the points and filter those points.
  std::vector<MergeLatticePoint> dominatedPoints;
  std::vector<Iterator> lpIterators = lp.getIterators();
  std::sort(lpIterators.begin(), lpIterators.end());
  for (auto& lq : *this) {
    std::vector<Iterator> lqIterators = lq.getIterators();
    std::sort(lqIterators.begin(), lqIterators.end());
    if (std::includes(lpIterators.begin(), lpIterators.end(),
                      lqIterators.begin(), lqIterators.end())) {
      dominatedPoints.push_back(lq);
    }
  }
  return MergeLattice(dominatedPoints, resultIterators);
}

bool MergeLattice::isFull() const {
  // A merge lattice is full if any lattice point iterates over a single full 
  // iterator or if each sparse iterator is uniquely iterated by some lattice 
  // point.
  std::set<Iterator> uniquelyMergedIterators;
  for (auto& point : *this) {
    if (point.getRangeIterators().size() == 1) {
      auto it = point.getRangeIterators()[0];
      uniquelyMergedIterators.insert(it);
      if (it.isFull()) {
        return true;
      }
    }
  }
  for (auto& it : getMergeIterators()) {
    if (!util::contains(uniquelyMergedIterators, it)) {
      return false;
    }
  }
  return true;
}

bool MergeLattice::defined() const {
  return points.size() > 0;
}

std::vector<MergeLatticePoint>::iterator MergeLattice::begin() {
  return points.begin();
}


std::vector<MergeLatticePoint>::iterator MergeLattice::end() {
  return points.end();
}

std::vector<MergeLatticePoint>::const_iterator MergeLattice::begin() const {
  return points.begin();
}


std::vector<MergeLatticePoint>::const_iterator MergeLattice::end() const {
  return points.end();
}

template<class op>
MergeLattice conjunction(MergeLattice a, MergeLattice b) {
  std::vector<MergeLatticePoint> points;

  // Append all combinations of a and b lattice points
  for (auto& aLatticePoint : a) {
    for (auto& bLatticePoint : b) {
      points.push_back(conjunction<op>(aLatticePoint, bLatticePoint));
    }
  }

  return MergeLattice(points, util::combine(a.getResultIterators(),
                                            b.getResultIterators()));
}

template<class op>
MergeLattice disjunction(MergeLattice a, MergeLattice b) {
  std::vector<MergeLatticePoint> points;

  // Append all combinations of the lattice points of a and b
  std::vector<MergeLatticePoint> allPoints;
  for (auto& aLatticePoint : a) {
    for (auto& bLatticePoint : b) {
      allPoints.push_back(disjunction<op>(aLatticePoint, bLatticePoint));
    }
  }

  // Append the lattice points of a
  util::append(allPoints, a);

  // Append the lattice points of b
  util::append(allPoints, b);

  taco_iassert(allPoints.size() > 0) <<
      "A lattice must have at least one point";

  // Exhausting an iterator over a full tensor mode cause the lattice to drop
  // to zero. Therefore we cannot end up in a lattice point that doesn't
  // contain the iterator over the full mode and must remove all lattice points
  // that don't contain it.
  auto fullIterators = old::getFullIterators(allPoints[0].getMergeIterators());
  for (auto& point : allPoints) {
    bool missingFullIterator = false;
    for (auto& fullIterator : fullIterators) {
      if (!util::contains(point.getMergeIterators(), fullIterator)) {
        missingFullIterator = true;
        break;
      }
    }
    if (!missingFullIterator) {
      points.push_back(point);
    }
  }

  MergeLattice lattice(points, util::combine(a.getResultIterators(),
                                             b.getResultIterators()));
  taco_iassert(lattice.getSize() > 0) <<
      "All lattices must have at least one point";
  return lattice;
}

std::ostream& operator<<(std::ostream& os, const MergeLattice& ml) {
  return os << util::join(ml, " \u2228 ");
}

bool operator==(const MergeLattice& a, const MergeLattice& b) {
  if (a.getSize() != b.getSize()) {
    return false;
  }
  for (size_t i = 0; i < a.getSize(); i++) {
    if (a[i] != b[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const MergeLattice& a, const MergeLattice& b) {
  return !(a == b);
}


// class MergeLatticePoint
MergeLatticePoint::MergeLatticePoint(std::vector<Iterator> iterators,
                                     std::vector<Iterator> mergeIters,
                                     std::vector<Iterator> rangeIters,
                                     IndexExpr expr)
    : iterators(iterators), mergeIterators(mergeIters), 
      rangeIterators(rangeIters), expr(expr) {
  taco_iassert(iterators.size() >= mergeIterators.size());
  taco_iassert(mergeIterators.size() >= rangeIterators.size());
}

const std::vector<Iterator>& MergeLatticePoint::getIterators() const {
  return iterators;
}

const std::vector<Iterator>& MergeLatticePoint::getMergeIterators() const {
  return mergeIterators;
}

const std::vector<Iterator>& MergeLatticePoint::getRangeIterators() const {
  return rangeIterators;
}

const IndexExpr& MergeLatticePoint::getExpr() const {
  return expr;
}

template<class op>
MergeLatticePoint merge(MergeLatticePoint a, MergeLatticePoint b,
                        bool conjunctive) {
  std::vector<Iterator> iters;
  iters.insert(iters.end(), a.getIterators().begin(), a.getIterators().end());
  iters.insert(iters.end(), b.getIterators().begin(), b.getIterators().end());

  IndexExpr expr = new op(a.getExpr(), b.getExpr());

  std::vector<Iterator> mergeItersIfKeepLHS = a.getMergeIterators();
  for (const auto& iter : b.getMergeIterators()) {
    if (!conjunctive || !iter.hasLocate()) {
      mergeItersIfKeepLHS.push_back(iter);
    }
  }

  std::vector<Iterator> mergeItersIfKeepRHS = b.getMergeIterators();
  for (const auto& iter : a.getMergeIterators()) {
    if (!conjunctive || !iter.hasLocate()) {
      mergeItersIfKeepRHS.push_back(iter);
    }
  }

  const auto rangeItersIfKeepLHS = simplify(mergeItersIfKeepLHS);
  const auto rangeItersIfKeepRHS = simplify(mergeItersIfKeepRHS);

  return (rangeItersIfKeepLHS.size() <= rangeItersIfKeepRHS.size()) ? 
      MergeLatticePoint(iters, mergeItersIfKeepLHS, rangeItersIfKeepLHS, expr) : 
      MergeLatticePoint(iters, mergeItersIfKeepRHS, rangeItersIfKeepRHS, expr);
}

template<class op>
MergeLatticePoint conjunction(MergeLatticePoint a, MergeLatticePoint b) {
  return merge<op>(a, b, true);
}

template<class op>
MergeLatticePoint disjunction(MergeLatticePoint a, MergeLatticePoint b) {
  return merge<op>(a, b, false);
}

std::ostream& operator<<(std::ostream& os, const MergeLatticePoint& mlp) {
  std::vector<std::string> pathNames;
  os << "[";
  os << util::join(mlp.getIterators(), " \u2227 ");
  os << "]";
  return os;
}

bool operator==(const MergeLatticePoint& a, const MergeLatticePoint& b) {
  auto& aiters = a.getIterators();
  auto& biters = b.getIterators();
  if (aiters.size() != biters.size()) {
    return false;
  }
  for (size_t i = 0; i < aiters.size(); i++) {
    if (aiters[i] != biters[i]) {
      return false;
    }
  }
  return true;
}

bool operator!=(const MergeLatticePoint& a, const MergeLatticePoint& b) {
  return !(a == b);
}

std::vector<Iterator> simplify(const std::vector<Iterator>& iterators) {
  std::vector<Iterator> simplifiedIterators;
  std::vector<Iterator> fullIterators;

  for (const auto& iter : iterators) {
    if (!iter.isFull()) {
      simplifiedIterators.push_back(iter);
    } else if (fullIterators.empty()) {
      // must iterate over at least one of the full dimensions
      fullIterators.push_back(iter);
    } else if (!iter.hasLocate()) {
      // preferably iterate over only full dimensions that do not support locate
      if (fullIterators[0].hasLocate()) {
        fullIterators.clear();
      }
      fullIterators.push_back(iter);
    }
  }
  util::append(simplifiedIterators, fullIterators);

  return simplifiedIterators;
}

std::set<Access> exhaustedAccesses(MergeLatticePoint lp, MergeLattice l) {
  std::set<Iterator> notExhaustedIters(lp.getIterators().begin(),
                                       lp.getIterators().end());
  std::set<Access> exhausted;
  for (auto& iter : l.getMergeIterators()) {
    if (!util::contains(notExhaustedIters, iter)) {
      exhausted.insert(iter.getTensorPath().getAccess());
    }
  }

  return exhausted;
}

}
