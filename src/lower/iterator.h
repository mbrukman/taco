#ifndef TACO_STORAGE_ITERATOR_H
#define TACO_STORAGE_ITERATOR_H

#include <memory>
#include <ostream>
#include <string>
#include <tuple>
#include <vector>

#include "taco/lower/mode.h"
#include "taco/ir/ir.h"
#include "taco/util/comparable.h"
#include "lower/tensor_path.h"

namespace taco {
class Type;

namespace ir {
class Stmt;
class Expr;
}

/// A compile-time iterator over a (tensor) `Mode`. This class can be used
/// to generate the IR expressions for accessing different level types.
class Iterator : public util::Comparable<Iterator> {
public:
  Iterator();

  /// Construct a root iterator.
  Iterator(const ir::Expr& tensorVar);

  /// Construct an iterator from an tensor path.
  /// @deprecated
  Iterator(const old::TensorPath& path, std::string coordVarName,
           const ir::Expr& tensor, Mode mode, Iterator parent);

  /// Construct a non-root iterator.
  Iterator(ir::Expr tensor, Mode mode, Iterator parent, std::string name);

  /// Get the tensor path this iterator list iterates over.
  /// TODO: Remove this method and the path field.
  const old::TensorPath& getTensorPath() const;

  /// Get the parent of this iterator in its iterator list.
  const Iterator& getParent() const;
  
  /// Returns the tensor this iterator is iterating over.
  ir::Expr getTensor() const;

  /// Returns reference to object identifying the tensor mode being iterated.
  const Mode& getMode() const;

  /// Returns the pos variable for this iterator (e.g. `pa1`). Ptr variables
  /// are used to index into the data at the next level (as well as the index
  /// arrays for formats such as sparse that have them).
  ir::Expr getPosVar() const;

  /// Returns the index variable for this iterator (e.g. `ja`). Index variables
  /// are merged together using `min` in the emitted code to produce the loop
  /// index variable (e.g. `j`).
  ir::Expr getIdxVar() const;

  /// Returns the pos variable if level supports coordinate position iteration, 
  /// or the index variable if the level supports coordinate value iteration.
  ir::Expr getIteratorVar() const;

  /// Returns the pos variable if level supports coordinate value iteration, 
  /// or the index variable if the level supports coordinate position iteration.
  ir::Expr getDerivedVar() const;

  /// Returns the variable that indicates the end bound for iteration.
  ir::Expr getEndVar() const;

  /// Returns the variable that indicates the end bound for positions in the 
  /// level that store the same coordinate.
  ir::Expr getSegendVar() const;

  /// Returns the variable that indicates whether an access is valid. E.g., for 
  /// a hashed level, this indicates whether a bucket stores a coordinate.
  ir::Expr getValidVar() const;

  /// Returns the variable that indicates the starting bound for positions of 
  /// coordinates appended to a level belonging to the same subtensor.
  ir::Expr getBeginVar() const;

  /// Properties of level being iterated.
  bool isFull() const;
  bool isOrdered() const; 
  bool isUnique() const;
  bool isBranchless() const; 
  bool isCompact() const; 

  /// Capabilities supported by levels being iterated.
  bool hasCoordValIter() const;
  bool hasCoordPosIter() const; 
  bool hasLocate() const;
  bool hasInsert() const;
  bool hasAppend() const;

  /// Return code for level functions that implement coordinate value iteration.
  ModeFunction coordIter(const std::vector<ir::Expr>& coords) const;
  ModeFunction coordAccess(const ir::Expr& parentPos,
                           const std::vector<ir::Expr>& coords) const;
  
  /// Return code for level functions that implement coordinate position  
  /// iteration.
  ModeFunction posIter(const ir::Expr& parentPos) const;
  ModeFunction posAccess(const ir::Expr& parentPos,
                         const std::vector<ir::Expr>& coords) const;
  
  /// Returns code for level function that implements locate capability.
  ModeFunction locate(const ir::Expr& parentPos,
                      const std::vector<ir::Expr>& coords) const;

  /// Return code for level functions that implement insert capabilitiy.
  ir::Stmt getInsertCoord(const ir::Expr& p, 
      const std::vector<ir::Expr>& i) const;
  ir::Expr getSize() const;
  ir::Stmt getInsertInitCoords(const ir::Expr& pBegin, 
      const ir::Expr& pEnd) const;
  ir::Stmt getInsertInitLevel(const ir::Expr& szPrev, const ir::Expr& sz) const;
  ir::Stmt getInsertFinalizeLevel(const ir::Expr& szPrev, 
      const ir::Expr& sz) const;
  
  /// Return code for level functions that implement append capabilitiy.
  ir::Stmt getAppendCoord(const ir::Expr& p, const ir::Expr& i) const; 
  ir::Stmt getAppendEdges(const ir::Expr& pPrev, const ir::Expr& pBegin, 
      const ir::Expr& pEnd) const;
  ir::Stmt getAppendInitEdges(const ir::Expr& pPrevBegin, 
      const ir::Expr& pPrevEnd) const;
  ir::Stmt getAppendInitLevel(const ir::Expr& szPrev, const ir::Expr& sz) const;
  ir::Stmt getAppendFinalizeLevel(const ir::Expr& szPrev, 
      const ir::Expr& sz) const;

  /// Returns true if the iterator is defined, false otherwise.
  bool defined() const;

  friend bool operator==(const Iterator&, const Iterator&);
  friend bool operator<(const Iterator&, const Iterator&);
  friend std::ostream& operator<<(std::ostream&, const Iterator&);

private:
  struct Content;
  std::shared_ptr<Content> content;
};

}
#endif