/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*                                                                       */
/*    This file is part of the HiGHS linear optimization suite           */
/*                                                                       */
/*    Written and engineered 2008-2021 at the University of Edinburgh    */
/*                                                                       */
/*    Available as open-source under the MIT License                     */
/*                                                                       */
/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/**@file lp_data/HighsInterface.cpp
 * @brief
 * @author Julian Hall, Ivet Galabova, Qi Huangfu and Michael Feldmeier
 */
#include "HConfig.h"
#include "Highs.h"
#include "lp_data/HighsLpUtils.h"
#include "simplex/HSimplex.h"
#include "util/HighsSort.h"

HighsStatus Highs::addColsInterface(int XnumNewCol, const double* XcolCost,
                                    const double* XcolLower,
                                    const double* XcolUpper, int XnumNewNZ,
                                    const int* XAstart, const int* XAindex,
                                    const double* XAvalue) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsStatus return_status = HighsStatus::OK;
  HighsOptions& options = highs_model_object.options_;
  if (XnumNewCol < 0) return HighsStatus::Error;
  if (XnumNewNZ < 0) return HighsStatus::Error;
  if (XnumNewCol == 0) return HighsStatus::OK;
  if (XnumNewCol > 0)
    if (isColDataNull(options.log_options, XcolCost, XcolLower, XcolUpper))
      return HighsStatus::Error;
  if (XnumNewNZ > 0)
    if (isMatrixDataNull(options.log_options, XAstart, XAindex, XAvalue))
      return HighsStatus::Error;

  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsScale& scale = highs_model_object.scale_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  HighsLp& simplex_lp = ekk_instance.simplex_lp_;
  SimplexBasis& simplex_basis = ekk_instance.simplex_basis_;

  bool& valid_basis = basis.valid_;
  bool& valid_simplex_lp = simplex_lp_status.valid;
  bool& valid_simplex_basis = simplex_lp_status.has_basis;
  bool& scaled_simplex_lp = scale.is_scaled_;

  // Check that if nonzeros are to be added then the model has a positive number
  // of rows
  if (lp.numRow_ <= 0 && XnumNewNZ > 0) return HighsStatus::Error;
  if (valid_simplex_lp && (simplex_lp.numRow_ <= 0 && XnumNewNZ > 0))
    return HighsStatus::Error;

  // Record the new number of columns
  int newNumCol = lp.numCol_ + XnumNewCol;

#ifdef HiGHSDEV
  // Check that if there is no simplex LP then there is no basis, matrix or
  // scaling
  if (!valid_simplex_lp) {
    assert(!scaled_simplex_lp);
  }
#endif

  HighsIndexCollection index_collection;
  index_collection.dimension_ = XnumNewCol;
  index_collection.is_interval_ = true;
  index_collection.from_ = 0;
  index_collection.to_ = XnumNewCol - 1;

  // Take a copy of the cost and bounds that can be normalised
  std::vector<double> local_colCost{XcolCost, XcolCost + XnumNewCol};
  std::vector<double> local_colLower{XcolLower, XcolLower + XnumNewCol};
  std::vector<double> local_colUpper{XcolUpper, XcolUpper + XnumNewCol};

  // There are sure to be new columns since XnumNewCol <= 0 is handled above
  // Assess the column costs
  assert(XnumNewCol > 0);
  return_status =
      interpretCallStatus(assessCosts(options, lp.numCol_, index_collection,
                                      local_colCost, options.infinite_cost),
                          return_status, "assessCosts");
  if (return_status == HighsStatus::Error) return return_status;
  // Assess the column bounds
  return_status = interpretCallStatus(
      assessBounds(options, "Col", lp.numCol_, index_collection, local_colLower,
                   local_colUpper, options.infinite_bound),
      return_status, "assessBounds");
  if (return_status == HighsStatus::Error) return return_status;
  // Append the columns to the LP vectors and matrix
  return_status =
      interpretCallStatus(appendColsToLpVectors(lp, XnumNewCol, local_colCost,
                                                local_colLower, local_colUpper),
                          return_status, "appendColsToLpVectors");
  if (return_status == HighsStatus::Error) return return_status;

  if (valid_simplex_lp) {
    // Append the columns to the Simplex LP vectors and matrix
    return_status = interpretCallStatus(
        appendColsToLpVectors(simplex_lp, XnumNewCol, local_colCost,
                              local_colLower, local_colUpper),
        return_status, "appendColsToLpVectors");
    if (return_status == HighsStatus::Error) return return_status;
  }

  // Now consider scaling. First resize the scaling factors and
  // initialise the new components
  scale.col_.resize(newNumCol);
  for (int col = 0; col < XnumNewCol; col++) scale.col_[lp.numCol_ + col] = 1.0;

  // Now consider any new matrix columns
  if (XnumNewNZ) {
    // There are nonzeros, so take a copy of the matrix that can be
    // normalised
    int local_num_new_nz = XnumNewNZ;
    std::vector<int> local_Astart{XAstart, XAstart + XnumNewCol};
    std::vector<int> local_Aindex{XAindex, XAindex + XnumNewNZ};
    std::vector<double> local_Avalue{XAvalue, XAvalue + XnumNewNZ};
    local_Astart.resize(XnumNewCol + 1);
    local_Astart[XnumNewCol] = XnumNewNZ;
    // Assess the matrix columns
    return_status = interpretCallStatus(
        assessMatrix(options, lp.numRow_, XnumNewCol, local_Astart,
                     local_Aindex, local_Avalue, options.small_matrix_value,
                     options.large_matrix_value),
        return_status, "assessMatrix");
    if (return_status == HighsStatus::Error) return return_status;
    local_num_new_nz = local_Astart[XnumNewCol];
    // Append the columns to the LP matrix
    return_status = interpretCallStatus(
        appendColsToLpMatrix(lp, XnumNewCol, local_num_new_nz, &local_Astart[0],
                             &local_Aindex[0], &local_Avalue[0]),
        return_status, "appendColsToLpMatrix");
    if (return_status == HighsStatus::Error) return return_status;
    if (valid_simplex_lp) {
      if (scaled_simplex_lp) {
        // Apply the row scaling to the new columns
        applyRowScalingToMatrix(scale.row_, XnumNewCol, local_Astart,
                                local_Aindex, local_Avalue);
        // Determine and apply the column scaling for the new columns
        colScaleMatrix(options.allowed_simplex_matrix_scale_factor,
                       &scale.col_[lp.numCol_], XnumNewCol, local_Astart,
                       local_Aindex, local_Avalue);
      }
      // Append the columns to the Simplex LP matrix
      return_status = interpretCallStatus(
          appendColsToLpMatrix(simplex_lp, XnumNewCol, local_num_new_nz,
                               &local_Astart[0], &local_Aindex[0],
                               &local_Avalue[0]),
          return_status, "appendColsToLpMatrix");
      if (return_status == HighsStatus::Error) return return_status;
      if (scaled_simplex_lp) {
        // Apply the column scaling to the costs and bounds
        HighsIndexCollection scaling_index_collection;
        scaling_index_collection.dimension_ = newNumCol;
        scaling_index_collection.is_interval_ = true;
        scaling_index_collection.from_ = lp.numCol_;
        scaling_index_collection.to_ = newNumCol - 1;
        return_status = interpretCallStatus(
            applyScalingToLpColCost(options.log_options, simplex_lp, scale.col_,
                                    scaling_index_collection),
            return_status, "applyScalingToLpColCost");
        if (return_status == HighsStatus::Error) return return_status;
        return_status = interpretCallStatus(
            applyScalingToLpColBounds(options.log_options, simplex_lp,
                                      scale.col_, scaling_index_collection),
            return_status, "applyScalingToLpColBounds");
        if (return_status == HighsStatus::Error) return return_status;
      }
    }
  } else {
    // There are no nonzeros, so XAstart/XAindex/XAvalue may be null. Have to
    // set up starts for empty columns
    assert(XnumNewCol > 0);
    appendColsToLpMatrix(lp, XnumNewCol, 0, NULL, NULL, NULL);
    if (valid_simplex_lp) {
      appendColsToLpMatrix(simplex_lp, XnumNewCol, 0, NULL, NULL, NULL);
      // Should be extendSimplexLpRandomVectors here
    }
  }
  // Update the basis correponding to new nonbasic columns
  if (valid_basis) appendNonbasicColsToBasis(lp, basis, XnumNewCol);
  if (valid_simplex_basis)
    appendNonbasicColsToBasis(simplex_lp, simplex_basis, XnumNewCol);

  // Deduce the consequences of adding new columns
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(simplex_lp_status, LpAction::NEW_COLS);

  // Increase the number of columns in the LPs
  lp.numCol_ += XnumNewCol;
  if (valid_simplex_lp) {
    simplex_lp.numCol_ += XnumNewCol;
    ekk_instance.initialiseSimplexLpRandomVectors();
  }

  return return_status;
}

HighsStatus Highs::addRowsInterface(int XnumNewRow, const double* XrowLower,
                                    const double* XrowUpper, int XnumNewNZ,
                                    const int* XARstart, const int* XARindex,
                                    const double* XARvalue) {
  // addRows is fundamentally different from addCols, since the new
  // matrix data are held row-wise, so we have to insert data into the
  // column-wise matrix of the LP.
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsStatus return_status = HighsStatus::OK;
  HighsOptions& options = highs_model_object.options_;
  if (XnumNewRow < 0) return HighsStatus::Error;
  if (XnumNewNZ < 0) return HighsStatus::Error;
  if (XnumNewRow == 0) return HighsStatus::OK;
  if (XnumNewRow > 0)
    if (isRowDataNull(options.log_options, XrowLower, XrowUpper))
      return HighsStatus::Error;
  if (XnumNewNZ > 0)
    if (isMatrixDataNull(options.log_options, XARstart, XARindex, XARvalue))
      return HighsStatus::Error;

  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsScale& scale = highs_model_object.scale_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  HighsLp& simplex_lp = ekk_instance.simplex_lp_;
  SimplexBasis& simplex_basis = ekk_instance.simplex_basis_;

  // Query: should simplex_lp_status.valid be simplex_lp_status.valid_?
  bool& valid_basis = basis.valid_;
  bool& valid_simplex_lp = simplex_lp_status.valid;
  bool& valid_simplex_basis = simplex_lp_status.has_basis;
  bool& scaled_simplex_lp = scale.is_scaled_;

  // Check that if nonzeros are to be added then the model has a positive number
  // of columns
  if (lp.numCol_ <= 0 && XnumNewNZ > 0) return HighsStatus::Error;
  if (valid_simplex_lp && (simplex_lp.numCol_ <= 0 && XnumNewNZ > 0))
    return HighsStatus::Error;

  // Record the new number of rows
  int newNumRow = lp.numRow_ + XnumNewRow;

#ifdef HiGHSDEV
  // Check that if there is no simplex LP then there is no basis, matrix or
  // scaling
  if (!valid_simplex_lp) {
    assert(!scaled_simplex_lp);
  }
#endif

  HighsIndexCollection index_collection;
  index_collection.dimension_ = XnumNewRow;
  index_collection.is_interval_ = true;
  index_collection.from_ = 0;
  index_collection.to_ = XnumNewRow - 1;
  // Take a copy of the bounds that can be normalised
  std::vector<double> local_rowLower{XrowLower, XrowLower + XnumNewRow};
  std::vector<double> local_rowUpper{XrowUpper, XrowUpper + XnumNewRow};

  return_status = interpretCallStatus(
      assessBounds(options, "Row", lp.numRow_, index_collection, local_rowLower,
                   local_rowUpper, options.infinite_bound),
      return_status, "assessBounds");
  if (return_status == HighsStatus::Error) return return_status;

  // Append the rows to the LP vectors
  return_status = interpretCallStatus(
      appendRowsToLpVectors(lp, XnumNewRow, local_rowLower, local_rowUpper),
      return_status, "appendRowsToLpVectors");
  if (return_status == HighsStatus::Error) return return_status;

  if (valid_simplex_lp) {
    // Append the rows to the Simplex LP vectors
    return_status = interpretCallStatus(
        appendRowsToLpVectors(simplex_lp, XnumNewRow, local_rowLower,
                              local_rowUpper),
        return_status, "appendRowsToLpVectors");
    if (return_status == HighsStatus::Error) return return_status;
  }

  // Now consider scaling. First resize the scaling factors and
  // initialise the new components
  scale.row_.resize(newNumRow);
  for (int row = 0; row < XnumNewRow; row++) scale.row_[lp.numRow_ + row] = 1.0;

  // Now consider any new matrix rows
  if (XnumNewNZ) {
    // There are nonzeros, so take a copy of the matrix that can be
    // normalised
    int local_num_new_nz = XnumNewNZ;
    std::vector<int> local_ARstart{XARstart, XARstart + XnumNewRow};
    std::vector<int> local_ARindex{XARindex, XARindex + XnumNewNZ};
    std::vector<double> local_ARvalue{XARvalue, XARvalue + XnumNewNZ};
    local_ARstart.resize(XnumNewRow + 1);
    local_ARstart[XnumNewRow] = XnumNewNZ;
    // Assess the matrix columns
    return_status = interpretCallStatus(
        assessMatrix(options, lp.numCol_, XnumNewRow, local_ARstart,
                     local_ARindex, local_ARvalue, options.small_matrix_value,
                     options.large_matrix_value),
        return_status, "assessMatrix");
    if (return_status == HighsStatus::Error) return return_status;
    local_num_new_nz = local_ARstart[XnumNewRow];
    // Append the rows to LP matrix
    return_status = interpretCallStatus(
        appendRowsToLpMatrix(lp, XnumNewRow, local_num_new_nz,
                             &local_ARstart[0], &local_ARindex[0],
                             &local_ARvalue[0]),
        return_status, "appendRowsToLpMatrix");
    if (return_status == HighsStatus::Error) return return_status;
    if (valid_simplex_lp) {
      if (scaled_simplex_lp) {
        // Apply the column scaling to the new rows
        applyRowScalingToMatrix(scale.col_, XnumNewRow, local_ARstart,
                                local_ARindex, local_ARvalue);
        // Determine and apply the row scaling for the new rows. Using
        // colScaleMatrix to take the row-wise matrix and then treat
        // it col-wise
        colScaleMatrix(options.allowed_simplex_matrix_scale_factor,
                       &scale.row_[lp.numRow_], XnumNewRow, local_ARstart,
                       local_ARindex, local_ARvalue);
      }
      // Append the rows to the Simplex LP matrix
      return_status = interpretCallStatus(
          appendRowsToLpMatrix(simplex_lp, XnumNewRow, local_num_new_nz,
                               &local_ARstart[0], &local_ARindex[0],
                               &local_ARvalue[0]),
          return_status, "appendRowsToLpMatrix");
      if (return_status == HighsStatus::Error) return return_status;
      // Should be extendSimplexLpRandomVectors
      if (scaled_simplex_lp) {
        // Apply the row scaling to the bounds
        HighsIndexCollection scaling_index_collection;
        scaling_index_collection.dimension_ = newNumRow;
        scaling_index_collection.is_interval_ = true;
        scaling_index_collection.from_ = lp.numRow_;
        scaling_index_collection.to_ = newNumRow - 1;
        return_status = interpretCallStatus(
            applyScalingToLpRowBounds(options.log_options, simplex_lp,
                                      scale.row_, scaling_index_collection),
            return_status, "applyScalingToLpRowBounds");
        if (return_status == HighsStatus::Error) return return_status;
      }
    }
  } else if (lp.orientation_ == MatrixOrientation::NONE ||
             lp.orientation_ == MatrixOrientation::ROWWISE) {
    // There are no nonzeros, so XARstart/XARindex/XARvalue may be null. Have to
    // set up starts for empty rows
    assert(XnumNewRow > 0);
    appendRowsToLpMatrix(lp, XnumNewRow, 0, NULL, NULL, NULL);
    if (valid_simplex_lp) {
      appendRowsToLpMatrix(simplex_lp, XnumNewRow, 0, NULL, NULL, NULL);
      // Should be extendSimplexLpRandomVectors here
    }
  }
  // Update the basis correponding to new basic rows
  if (valid_basis) appendBasicRowsToBasis(lp, basis, XnumNewRow);
  if (valid_simplex_basis)
    appendBasicRowsToBasis(simplex_lp, simplex_basis, XnumNewRow);

  // Deduce the consequences of adding new rows
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(simplex_lp_status, LpAction::NEW_ROWS);

  // Increase the number of rows in the LPs
  lp.numRow_ += XnumNewRow;
  if (valid_simplex_lp) {
    simplex_lp.numRow_ += XnumNewRow;
    ekk_instance.initialiseSimplexLpRandomVectors();
  }

  return return_status;
}

HighsStatus Highs::deleteColsInterface(HighsIndexCollection& index_collection) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsOptions& options = highs_model_object.options_;
  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  // Query: should simplex_lp_status.valid be simplex_lp_status.valid_?
  // Ensure that the LP (and any simplex LP) is column-wise
  setOrientation(lp_);
  if (simplex_lp_status.valid) setOrientation(ekk_instance.simplex_lp_);
  assert(&lp_ == &lp);

  bool& valid_simplex_lp = simplex_lp_status.valid;
  // Keep a copy of the original number of columns to check whether
  // any columns have been removed, and if there is mask to be updated
  int original_num_col = lp.numCol_;

  HighsStatus return_status;
  return_status = deleteLpCols(options.log_options, lp, index_collection);
  if (return_status != HighsStatus::OK) return return_status;
  assert(lp.numCol_ <= original_num_col);
  if (lp.numCol_ < original_num_col) {
    // Nontrivial deletion so reset the model_status and invalidate
    // the Highs basis
    highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
    highs_model_object.unscaled_model_status_ =
        highs_model_object.scaled_model_status_;
    basis.valid_ = false;
  }
  return_status = interpretCallStatus(
      deleteScale(options.log_options, highs_model_object.scale_.col_,
                  index_collection),
      return_status, "deleteScale");
  if (return_status == HighsStatus::Error) return return_status;
  highs_model_object.scale_.col_.resize(lp.numCol_);
  if (valid_simplex_lp) {
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    return_status =
        deleteLpCols(options.log_options, simplex_lp, index_collection);
    if (return_status != HighsStatus::OK) return return_status;
    assert(simplex_lp.numCol_ <= original_num_col);
    if (simplex_lp.numCol_ < original_num_col) {
      // Nontrivial deletion so initialise the random vectors and all
      // data relating to the simplex basis
      ekk_instance.initialiseSimplexLpRandomVectors();
      invalidateSimplexLpBasis(simplex_lp_status);
    }
  }
  if (index_collection.is_mask_) {
    // Set the mask values to indicate the new index value of the
    // remaining columns
    int new_col = 0;
    for (int col = 0; col < original_num_col; col++) {
      if (!index_collection.mask_[col]) {
        index_collection.mask_[col] = new_col;
        new_col++;
      } else {
        index_collection.mask_[col] = -1;
      }
    }
    assert(new_col == lp.numCol_);
  }
  return HighsStatus::OK;
}

HighsStatus Highs::deleteRowsInterface(HighsIndexCollection& index_collection) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsOptions& options = highs_model_object.options_;
  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  // Query: should simplex_lp_status.valid be simplex_lp_status.valid_?
  // Ensure that the LP (and any simplex LP) is column-wise
  setOrientation(lp_);
  if (simplex_lp_status.valid) setOrientation(ekk_instance.simplex_lp_);
  assert(&lp_ == &lp);

  bool& valid_simplex_lp = simplex_lp_status.valid;
  // Keep a copy of the original number of rows to check whether
  // any rows have been removed, and if there is mask to be updated
  int original_num_row = lp.numRow_;

  HighsStatus return_status;
  return_status = deleteLpRows(options.log_options, lp, index_collection);
  if (return_status != HighsStatus::OK) return return_status;
  assert(lp.numRow_ <= original_num_row);
  if (lp.numRow_ < original_num_row) {
    // Nontrivial deletion so reset the model_status and invalidate
    // the Highs basis
    highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
    highs_model_object.unscaled_model_status_ =
        highs_model_object.scaled_model_status_;
    basis.valid_ = false;
  }
  return_status = interpretCallStatus(
      deleteScale(options.log_options, highs_model_object.scale_.row_,
                  index_collection),
      return_status, "deleteScale");
  if (return_status == HighsStatus::Error) return return_status;

  highs_model_object.scale_.row_.resize(lp.numRow_);
  if (valid_simplex_lp) {
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    return_status =
        deleteLpRows(options.log_options, simplex_lp, index_collection);
    if (return_status != HighsStatus::OK) return return_status;
    assert(simplex_lp.numRow_ <= original_num_row);
    if (simplex_lp.numRow_ < original_num_row) {
      // Nontrivial deletion so initialise the random vectors and all
      // data relating to the simplex basis
      ekk_instance.initialiseSimplexLpRandomVectors();
      invalidateSimplexLpBasis(simplex_lp_status);
    }
  }
  if (index_collection.is_mask_) {
    int new_row = 0;
    for (int row = 0; row < original_num_row; row++) {
      if (!index_collection.mask_[row]) {
        index_collection.mask_[row] = new_row;
        new_row++;
      } else {
        index_collection.mask_[row] = -1;
      }
    }
    assert(new_row == lp.numRow_);
  }
  return HighsStatus::OK;
}

HighsStatus Highs::getColsInterface(
    const HighsIndexCollection& index_collection, int& num_col,
    double* col_cost, double* col_lower, double* col_upper, int& num_nz,
    int* col_matrix_start, int* col_matrix_index, double* col_matrix_value) {
  HighsModelObject& highs_model_object = hmos_[0];
  HighsStatus return_status = HighsStatus::OK;
  HighsStatus call_status;
  HighsLp& lp = lp_;
  HighsOptions& options = highs_model_object.options_;
  // Ensure that the LP is column-wise
  setOrientation(lp_);
  assert(&lp_ == &lp);
  if (!assessIndexCollection(options.log_options, index_collection))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "assessIndexCollection");
  int from_k;
  int to_k;
  if (!limitsForIndexCollection(options.log_options, index_collection, from_k,
                                to_k))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "limitsForIndexCollection");
  if (from_k < 0 || to_k > lp.numCol_) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status, "getCols");
    return return_status;
  }
  if (from_k > to_k) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status, "getCols");
    return return_status;
  }
  int out_from_col;
  int out_to_col;
  int in_from_col;
  int in_to_col = -1;
  int current_set_entry = 0;
  int col_dim = lp.numCol_;
  // Ensure that the matrix is column-wise
  setOrientation(lp);
  num_col = 0;
  num_nz = 0;
  for (int k = from_k; k <= to_k; k++) {
    updateIndexCollectionOutInIndex(index_collection, out_from_col, out_to_col,
                                    in_from_col, in_to_col, current_set_entry);
    assert(out_to_col < col_dim);
    assert(in_to_col < col_dim);
    for (int col = out_from_col; col <= out_to_col; col++) {
      if (col_cost != NULL) col_cost[num_col] = lp.colCost_[col];
      if (col_lower != NULL) col_lower[num_col] = lp.colLower_[col];
      if (col_upper != NULL) col_upper[num_col] = lp.colUpper_[col];
      if (col_matrix_start != NULL)
        col_matrix_start[num_col] =
            num_nz + lp.Astart_[col] - lp.Astart_[out_from_col];
      num_col++;
    }
    for (int el = lp.Astart_[out_from_col]; el < lp.Astart_[out_to_col + 1];
         el++) {
      if (col_matrix_index != NULL) col_matrix_index[num_nz] = lp.Aindex_[el];
      if (col_matrix_value != NULL) col_matrix_value[num_nz] = lp.Avalue_[el];
      num_nz++;
    }
    if (out_to_col == col_dim - 1 || in_to_col == col_dim - 1) break;
  }
  return HighsStatus::OK;
}

HighsStatus Highs::getRowsInterface(
    const HighsIndexCollection& index_collection, int& num_row,
    double* row_lower, double* row_upper, int& num_nz, int* row_matrix_start,
    int* row_matrix_index, double* row_matrix_value) {
  HighsModelObject& highs_model_object = hmos_[0];
  HighsStatus return_status = HighsStatus::OK;
  HighsStatus call_status;
  HighsLp& lp = lp_;
  HighsOptions& options = highs_model_object.options_;
  // Ensure that the LP is column-wise
  setOrientation(lp_);
  assert(&lp_ == &lp);
  if (!assessIndexCollection(options.log_options, index_collection))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "assessIndexCollection");
  int from_k;
  int to_k;
  if (!limitsForIndexCollection(options.log_options, index_collection, from_k,
                                to_k))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "limitsForIndexCollection");
  if (from_k < 0 || to_k > lp.numRow_) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status, "getCols");
    return return_status;
  }
  num_row = 0;
  num_nz = 0;
  if (from_k > to_k) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status, "getCols");
    return return_status;
  }
  // "Out" means not in the set to be extrated
  // "In" means in the set to be extrated
  int out_from_row;
  int out_to_row;
  int in_from_row;
  int in_to_row = -1;
  int current_set_entry = 0;
  int row_dim = lp.numRow_;
  // Ensure that the matrix is column-wise
  setOrientation(lp);

  // Set up a row mask so that entries to be got from the column-wise
  // matrix can be identified and have their correct row index.
  vector<int> new_index;
  new_index.resize(lp.numRow_);

  if (!index_collection.is_mask_) {
    out_to_row = -1;
    current_set_entry = 0;
    for (int k = from_k; k <= to_k; k++) {
      updateIndexCollectionOutInIndex(index_collection, in_from_row, in_to_row,
                                      out_from_row, out_to_row,
                                      current_set_entry);
      if (k == from_k) {
        // Account for any initial rows not being extracted
        for (int row = 0; row < in_from_row; row++) {
          new_index[row] = -1;
        }
      }
      for (int row = in_from_row; row <= in_to_row; row++) {
        new_index[row] = num_row;
        num_row++;
      }
      for (int row = out_from_row; row <= out_to_row; row++) {
        new_index[row] = -1;
      }
      if (out_to_row >= row_dim - 1) break;
    }
  } else {
    for (int row = 0; row < lp.numRow_; row++) {
      if (index_collection.mask_[row]) {
        new_index[row] = num_row;
        num_row++;
      } else {
        new_index[row] = -1;
      }
    }
  }

  // Bail out if no rows are to be extracted
  if (num_row == 0) return HighsStatus::OK;

  // Allocate an array of lengths for the row-wise matrix to be extracted
  vector<int> row_matrix_length;
  row_matrix_length.resize(num_row);

  for (int row = 0; row < lp.numRow_; row++) {
    int new_row = new_index[row];
    if (new_row >= 0) {
      assert(new_row < num_row);
      if (row_lower != NULL) row_lower[new_row] = lp.rowLower_[row];
      if (row_upper != NULL) row_upper[new_row] = lp.rowUpper_[row];
      row_matrix_length[new_row] = 0;
    }
  }
  // Identify the lengths of the rows in the row-wise matrix to be extracted
  for (int col = 0; col < lp.numCol_; col++) {
    for (int el = lp.Astart_[col]; el < lp.Astart_[col + 1]; el++) {
      int row = lp.Aindex_[el];
      int new_row = new_index[row];
      if (new_row >= 0) row_matrix_length[new_row]++;
    }
  }

  if (row_matrix_start == NULL) {
    // If the matrix start vector is null then don't get values of
    // indices, otherwise both are meaningless
    if (row_matrix_index != NULL || row_matrix_value != NULL) {
      highsLogUser(highs_model_object.options_.log_options, HighsLogType::ERROR,
                   "Cannot supply meaningful row matrix indices/values with "
                   "null starts\n");
      return HighsStatus::Error;
    }
  } else {
    row_matrix_start[0] = 0;
    for (int row = 0; row < num_row - 1; row++) {
      row_matrix_start[row + 1] =
          row_matrix_start[row] + row_matrix_length[row];
    }

    // Fill the row-wise matrix with indices and values
    for (int col = 0; col < lp.numCol_; col++) {
      for (int el = lp.Astart_[col]; el < lp.Astart_[col + 1]; el++) {
        int row = lp.Aindex_[el];
        int new_row = new_index[row];
        if (new_row >= 0) {
          int row_el = row_matrix_start[new_row];
          if (row_matrix_index != NULL) row_matrix_index[row_el] = col;
          if (row_matrix_value != NULL)
            row_matrix_value[row_el] = lp.Avalue_[el];
          row_matrix_start[new_row]++;
        }
      }
    }
    // Restore the starts of the row-wise matrix and count the number of
    // nonzeros in it
    num_nz = 0;
    row_matrix_start[0] = 0;
    for (int row = 0; row < num_row - 1; row++) {
      row_matrix_start[row + 1] =
          row_matrix_start[row] + row_matrix_length[row];
      num_nz += row_matrix_length[row];
    }
    num_nz += row_matrix_length[num_row - 1];
  }
  return HighsStatus::OK;
}

HighsStatus Highs::getCoefficientInterface(const int Xrow, const int Xcol,
                                           double& value) {
  if (Xrow < 0 || Xrow >= lp_.numRow_) return HighsStatus::Error;
  if (Xcol < 0 || Xcol >= lp_.numCol_) return HighsStatus::Error;
  value = 0;
  // Ensure that the LP is column-wise
  setOrientation(lp_);
  for (int el = lp_.Astart_[Xcol]; el < lp_.Astart_[Xcol + 1]; el++) {
    if (lp_.Aindex_[el] == Xrow) {
      value = lp_.Avalue_[el];
      break;
    }
  }
  return HighsStatus::OK;
}

HighsStatus Highs::changeObjectiveSenseInterface(const ObjSense Xsense) {
  HighsModelObject& highs_model_object = hmos_[0];
  assert(&lp_ == &lp_);
  // If the sense doesn't change, just return
  if ((Xsense == ObjSense::MINIMIZE) == (lp_.sense_ == ObjSense::MINIMIZE))
    return HighsStatus::OK;
  // Assume that objective sense changes
  // Set the LP objective sense
  lp_.sense_ = Xsense;
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  // Set any Simplex LP objective sense
  if (highs_model_object.ekk_instance_.simplex_lp_status_.valid)
    highs_model_object.ekk_instance_.simplex_lp_.sense_ = Xsense;
  return HighsStatus::OK;
}

HighsStatus Highs::changeCostsInterface(HighsIndexCollection& index_collection,
                                        const double* usr_col_cost) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsOptions& options = highs_model_object.options_;
  bool null_data =
      doubleUserDataNotNull(options.log_options, usr_col_cost, "column costs");
  if (null_data) return HighsStatus::Error;
  int num_usr_col_cost = dataSizeOfIndexCollection(index_collection);
  // If a non-positive number of costs (may) need changing nothing needs to be
  // done
  if (num_usr_col_cost <= 0) return HighsStatus::OK;
  // Take a copy of the cost that can be normalised
  std::vector<double> local_colCost{usr_col_cost,
                                    usr_col_cost + num_usr_col_cost};
  // If changing the costs for a set of columns, ensure that the
  // set and data are in ascending order
  if (index_collection.is_set_)
    sortSetData(index_collection.set_num_entries_, index_collection.set_,
                usr_col_cost, NULL, NULL, &local_colCost[0], NULL, NULL);
  HighsLp& lp = lp_;
  HighsStatus return_status = HighsStatus::OK;
  return_status =
      interpretCallStatus(assessCosts(options, 0, index_collection,
                                      local_colCost, options.infinite_cost),
                          return_status, "assessCosts");
  if (return_status == HighsStatus::Error) return return_status;

  HighsStatus call_status =
      changeLpCosts(options.log_options, lp, index_collection, local_colCost);
  if (call_status == HighsStatus::Error) return HighsStatus::Error;

  if (ekk_instance.simplex_lp_status_.valid) {
    // Also change the simplex LP's costs
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    assert(lp.numCol_ == simplex_lp.numCol_);
    assert(lp.numRow_ == simplex_lp.numRow_);
    call_status = changeLpCosts(options.log_options, simplex_lp,
                                index_collection, local_colCost);
    if (call_status == HighsStatus::Error) return HighsStatus::Error;
    if (highs_model_object.scale_.is_scaled_) {
      applyScalingToLpColCost(options.log_options, simplex_lp,
                              highs_model_object.scale_.col_, index_collection);
    }
  }
  // Deduce the consequences of new costs
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(ekk_instance.simplex_lp_status_, LpAction::NEW_COSTS);
  return HighsStatus::OK;
}

HighsStatus Highs::changeColBoundsInterface(
    HighsIndexCollection& index_collection, const double* usr_col_lower,
    const double* usr_col_upper) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsOptions& options = highs_model_object.options_;
  bool null_data = false;
  null_data = doubleUserDataNotNull(options.log_options, usr_col_lower,
                                    "column lower bounds") ||
              null_data;
  null_data = doubleUserDataNotNull(options.log_options, usr_col_upper,
                                    "column upper bounds") ||
              null_data;
  if (null_data) return HighsStatus::Error;
  int num_usr_col_bounds = dataSizeOfIndexCollection(index_collection);
  // If a non-positive number of costs (may) need changing nothing needs to be
  // done
  if (num_usr_col_bounds <= 0) return HighsStatus::OK;
  // Take a copy of the cost that can be normalised
  std::vector<double> local_colLower{usr_col_lower,
                                     usr_col_lower + num_usr_col_bounds};
  std::vector<double> local_colUpper{usr_col_upper,
                                     usr_col_upper + num_usr_col_bounds};
  // If changing the bounds for a set of columns, ensure that the
  // set and data are in ascending order
  if (index_collection.is_set_)
    sortSetData(index_collection.set_num_entries_, index_collection.set_,
                usr_col_lower, usr_col_upper, NULL, &local_colLower[0],
                &local_colUpper[0], NULL);
  HighsLp& lp = lp_;
  HighsStatus return_status = HighsStatus::OK;
  return_status = interpretCallStatus(
      assessBounds(options, "col", 0, index_collection, local_colLower,
                   local_colUpper, options.infinite_bound),
      return_status, "assessBounds");
  if (return_status == HighsStatus::Error) return return_status;

  HighsStatus call_status =
      changeLpColBounds(options.log_options, lp, index_collection,
                        local_colLower, local_colUpper);
  if (call_status == HighsStatus::Error) return HighsStatus::Error;

  if (ekk_instance.simplex_lp_status_.valid) {
    // Also change the simplex LP's column bounds
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    assert(lp.numCol_ == simplex_lp.numCol_);
    assert(lp.numRow_ == simplex_lp.numRow_);
    call_status =
        changeLpColBounds(options.log_options, simplex_lp, index_collection,
                          local_colLower, local_colUpper);
    if (call_status == HighsStatus::Error) return HighsStatus::Error;
    if (highs_model_object.scale_.is_scaled_) {
      applyScalingToLpColBounds(options.log_options, simplex_lp,
                                highs_model_object.scale_.col_,
                                index_collection);
    }
  }
  if (highs_model_object.basis_.valid_) {
    // Update HiGHS basis status and (any) simplex move status of
    // nonbasic variables whose bounds have changed
    return_status =
        interpretCallStatus(setNonbasicStatusInterface(index_collection, true),
                            return_status, "setNonbasicStatusInterface");
    if (return_status == HighsStatus::Error) return return_status;
  }

  // Deduce the consequences of new col bounds
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(ekk_instance.simplex_lp_status_, LpAction::NEW_BOUNDS);
  return HighsStatus::OK;
}

HighsStatus Highs::changeRowBoundsInterface(
    HighsIndexCollection& index_collection, const double* usr_row_lower,
    const double* usr_row_upper) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsOptions& options = highs_model_object.options_;
  bool null_data = false;
  null_data = doubleUserDataNotNull(options.log_options, usr_row_lower,
                                    "row lower bounds") ||
              null_data;
  null_data = doubleUserDataNotNull(options.log_options, usr_row_upper,
                                    "row upper bounds") ||
              null_data;
  if (null_data) return HighsStatus::Error;
  int num_usr_row_bounds = dataSizeOfIndexCollection(index_collection);
  // If a non-positive number of costs (may) need changing nothing needs to be
  // done
  if (num_usr_row_bounds <= 0) return HighsStatus::OK;
  // Take a copy of the cost that can be normalised
  std::vector<double> local_rowLower{usr_row_lower,
                                     usr_row_lower + num_usr_row_bounds};
  std::vector<double> local_rowUpper{usr_row_upper,
                                     usr_row_upper + num_usr_row_bounds};
  // If changing the bounds for a set of rows, ensure that the
  // set and data are in ascending order
  if (index_collection.is_set_)
    sortSetData(index_collection.set_num_entries_, index_collection.set_,
                usr_row_lower, usr_row_upper, NULL, &local_rowLower[0],
                &local_rowUpper[0], NULL);
  HighsLp& lp = lp_;
  HighsStatus return_status = HighsStatus::OK;
  return_status = interpretCallStatus(
      assessBounds(options, "row", 0, index_collection, local_rowLower,
                   local_rowUpper, options.infinite_bound),
      return_status, "assessBounds");
  if (return_status == HighsStatus::Error) return return_status;

  HighsStatus call_status;
  call_status = changeLpRowBounds(options.log_options, lp, index_collection,
                                  local_rowLower, local_rowUpper);
  if (call_status == HighsStatus::Error) return HighsStatus::Error;

  if (ekk_instance.simplex_lp_status_.valid) {
    // Also change the simplex LP's row bounds
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    assert(lp.numCol_ == simplex_lp.numCol_);
    assert(lp.numRow_ == simplex_lp.numRow_);
    call_status =
        changeLpRowBounds(options.log_options, simplex_lp, index_collection,
                          local_rowLower, local_rowUpper);
    if (call_status == HighsStatus::Error) return HighsStatus::Error;
    if (highs_model_object.scale_.is_scaled_) {
      applyScalingToLpRowBounds(options.log_options, simplex_lp,
                                highs_model_object.scale_.row_,
                                index_collection);
    }
  }
  if (highs_model_object.basis_.valid_) {
    // Update HiGHS basis status and (any) simplex move status of
    // nonbasic variables whose bounds have changed
    return_status =
        interpretCallStatus(setNonbasicStatusInterface(index_collection, false),
                            return_status, "setNonbasicStatusInterface");
    if (return_status == HighsStatus::Error) return return_status;
  }
  // Deduce the consequences of new row bounds
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(ekk_instance.simplex_lp_status_, LpAction::NEW_BOUNDS);
  return HighsStatus::OK;
}

// Change a single coefficient in the matrix
HighsStatus Highs::changeCoefficientInterface(const int Xrow, const int Xcol,
                                              const double XnewValue) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsLp& lp = lp_;
  // Ensure that the LP (and any simplex LP) has the matrix column-wise
  setOrientation(lp_);
  if (highs_model_object.ekk_instance_.simplex_lp_status_.valid)
    setOrientation(highs_model_object.ekk_instance_.simplex_lp_);
  assert(&lp_ == &lp);
  if (Xrow < 0 || Xrow >= lp.numRow_) return HighsStatus::Error;
  if (Xcol < 0 || Xcol >= lp.numCol_) return HighsStatus::Error;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  bool& valid_simplex_lp = simplex_lp_status.valid;
  // Check that if there is no simplex LP then there is no matrix or scaling
  if (!valid_simplex_lp) {
    assert(!simplex_lp_status.has_matrix);
    assert(!highs_model_object.scale_.is_scaled_);
  }
  changeLpMatrixCoefficient(lp, Xrow, Xcol, XnewValue);
  if (valid_simplex_lp) {
    HighsLp& simplex_lp = ekk_instance.simplex_lp_;
    HighsScale& scale = highs_model_object.scale_;
    double scaledXnewValue = XnewValue * scale.row_[Xrow] * scale.col_[Xcol];
    changeLpMatrixCoefficient(simplex_lp, Xrow, Xcol, scaledXnewValue);
  }
  // Deduce the consequences of a changed element
  // ToDo: Can do something more intelligent if element is in nonbasic column.
  // Otherwise, treat it as if it's a new row
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(simplex_lp_status, LpAction::NEW_ROWS);
  return HighsStatus::OK;
}

HighsStatus Highs::scaleColInterface(const int col, const double scaleval) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsStatus return_status = HighsStatus::OK;
  HighsOptions& options = highs_model_object.options_;
  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  HighsLp& simplex_lp = ekk_instance.simplex_lp_;
  SimplexBasis& simplex_basis = ekk_instance.simplex_basis_;

  // Ensure that the LP (and any simplex LP) is column-wise
  setOrientation(lp_);
  if (simplex_lp_status.valid) setOrientation(ekk_instance.simplex_lp_);
  assert(&lp_ == &lp);

  return_status = interpretCallStatus(
      applyScalingToLpCol(options.log_options, lp, col, scaleval),
      return_status, "applyScalingToLpCol");
  if (return_status == HighsStatus::Error) return return_status;

  if (scaleval < 0 && basis.valid_) {
    // Negative, so flip any nonbasic status
    if (basis.col_status[col] == HighsBasisStatus::LOWER) {
      basis.col_status[col] = HighsBasisStatus::UPPER;
    } else if (basis.col_status[col] == HighsBasisStatus::UPPER) {
      basis.col_status[col] = HighsBasisStatus::LOWER;
    }
  }
  if (simplex_lp_status.valid) {
    // Apply the scaling to the simplex LP
    return_status = interpretCallStatus(
        applyScalingToLpCol(options.log_options, simplex_lp, col, scaleval),
        return_status, "applyScalingToLpCol");
    if (return_status == HighsStatus::Error) return return_status;
    if (scaleval < 0 && simplex_lp_status.has_basis) {
      // Negative, so flip any nonbasic status
      if (simplex_basis.nonbasicMove_[col] == NONBASIC_MOVE_UP) {
        simplex_basis.nonbasicMove_[col] = NONBASIC_MOVE_DN;
      } else if (simplex_basis.nonbasicMove_[col] == NONBASIC_MOVE_DN) {
        simplex_basis.nonbasicMove_[col] = NONBASIC_MOVE_UP;
      }
    }
  }

  // Deduce the consequences of a scaled column
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(simplex_lp_status, LpAction::SCALED_COL);
  return HighsStatus::OK;
}

HighsStatus Highs::scaleRowInterface(const int row, const double scaleval) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsStatus return_status = HighsStatus::OK;
  HighsOptions& options = highs_model_object.options_;
  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  HighsLp& simplex_lp = ekk_instance.simplex_lp_;
  SimplexBasis& simplex_basis = ekk_instance.simplex_basis_;

  // Ensure that the LP (and any simplex LP) is column-wise
  setOrientation(lp_);
  if (simplex_lp_status.valid) setOrientation(ekk_instance.simplex_lp_);
  assert(&lp_ == &lp);

  return_status = interpretCallStatus(
      applyScalingToLpRow(options.log_options, lp, row, scaleval),
      return_status, "applyScalingToLpRow");
  if (return_status == HighsStatus::Error) return return_status;

  if (scaleval < 0 && basis.valid_) {
    // Negative, so flip any nonbasic status
    if (basis.row_status[row] == HighsBasisStatus::LOWER) {
      basis.row_status[row] = HighsBasisStatus::UPPER;
    } else if (basis.row_status[row] == HighsBasisStatus::UPPER) {
      basis.row_status[row] = HighsBasisStatus::LOWER;
    }
  }
  if (simplex_lp_status.valid) {
    // Apply the scaling to the simplex LP
    return_status = interpretCallStatus(
        applyScalingToLpRow(options.log_options, simplex_lp, row, scaleval),
        return_status, "applyScalingToLpRow");
    if (return_status == HighsStatus::Error) return return_status;
    if (scaleval < 0 && simplex_lp_status.has_basis) {
      // Negative, so flip any nonbasic status
      const int var = simplex_lp.numCol_ + row;
      if (simplex_basis.nonbasicMove_[var] == NONBASIC_MOVE_UP) {
        simplex_basis.nonbasicMove_[var] = NONBASIC_MOVE_DN;
      } else if (simplex_basis.nonbasicMove_[var] == NONBASIC_MOVE_DN) {
        simplex_basis.nonbasicMove_[var] = NONBASIC_MOVE_UP;
      }
    }
  }

  // Deduce the consequences of a scaled row
  highs_model_object.scaled_model_status_ = HighsModelStatus::NOTSET;
  highs_model_object.unscaled_model_status_ =
      highs_model_object.scaled_model_status_;
  updateSimplexLpStatus(simplex_lp_status, LpAction::SCALED_ROW);
  return HighsStatus::OK;
}

HighsStatus Highs::setNonbasicStatusInterface(
    const HighsIndexCollection& index_collection, const bool columns) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsStatus return_status = HighsStatus::OK;
  HighsStatus call_status;
  HighsLp& lp = lp_;
  HighsBasis& basis = highs_model_object.basis_;
  SimplexBasis& simplex_basis = ekk_instance.simplex_basis_;
  HighsOptions& options = highs_model_object.options_;

  assert(basis.valid_);
  const bool has_simplex_basis = ekk_instance.simplex_lp_status_.has_basis;

  if (!assessIndexCollection(options.log_options, index_collection))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "assessIndexCollection");
  int from_k;
  int to_k;
  if (!limitsForIndexCollection(options.log_options, index_collection, from_k,
                                to_k))
    return interpretCallStatus(HighsStatus::Error, return_status,
                               "limitsForIndexCollection");
  int ix_dim;
  if (columns) {
    ix_dim = lp.numCol_;
  } else {
    ix_dim = lp.numRow_;
  }
  if (from_k < 0 || to_k > ix_dim) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status,
                                        "setNonbasicStatusInterface");
    return return_status;
  }
  if (from_k > to_k) {
    call_status = HighsStatus::Error;
    return_status = interpretCallStatus(call_status, return_status,
                                        "setNonbasicStatusInterface");
    return return_status;
  }
  int set_from_ix;
  int set_to_ix;
  int ignore_from_ix;
  int ignore_to_ix = -1;
  int current_set_entry = 0;
  const int illegal_move_value = -99;
  for (int k = from_k; k <= to_k; k++) {
    updateIndexCollectionOutInIndex(index_collection, set_from_ix, set_to_ix,
                                    ignore_from_ix, ignore_to_ix,
                                    current_set_entry);
    assert(set_to_ix < ix_dim);
    assert(ignore_to_ix < ix_dim);
    if (columns) {
      for (int iCol = set_from_ix; iCol <= set_to_ix; iCol++) {
        if (basis.col_status[iCol] == HighsBasisStatus::BASIC) continue;
        // Nonbasic column
        double lower = lp.colLower_[iCol];
        double upper = lp.colUpper_[iCol];
        if (!highs_isInfinity(-lower)) {
          basis.col_status[iCol] = HighsBasisStatus::LOWER;
        } else if (!highs_isInfinity(upper)) {
          basis.col_status[iCol] = HighsBasisStatus::UPPER;
        } else {
          basis.col_status[iCol] = HighsBasisStatus::ZERO;
        }
        if (has_simplex_basis) {
          assert(simplex_basis.nonbasicFlag_[iCol] == NONBASIC_FLAG_TRUE);
          int move = illegal_move_value;
          if (lower == upper) {
            move = NONBASIC_MOVE_ZE;
          } else if (!highs_isInfinity(-lower)) {
            // Finite lower bound so boxed or lower
            if (!highs_isInfinity(upper)) {
              // Finite upper bound so boxed
              if (fabs(lower) < fabs(upper)) {
                move = NONBASIC_MOVE_UP;
              } else {
                move = NONBASIC_MOVE_DN;
              }
            } else {
              // Lower (since upper bound is infinite)
              move = NONBASIC_MOVE_UP;
            }
          } else if (!highs_isInfinity(upper)) {
            // Upper
            move = NONBASIC_MOVE_DN;
          } else {
            // FREE
            move = NONBASIC_MOVE_ZE;
          }
          assert(move != illegal_move_value);
          simplex_basis.nonbasicMove_[iCol] = move;
        }
      }
    } else {
      for (int iRow = set_from_ix; iRow <= set_to_ix; iRow++) {
        if (basis.row_status[iRow] == HighsBasisStatus::BASIC) continue;
        // Nonbasic column
        double lower = lp.rowLower_[iRow];
        double upper = lp.rowUpper_[iRow];
        if (!highs_isInfinity(-lower)) {
          basis.row_status[iRow] = HighsBasisStatus::LOWER;
        } else if (!highs_isInfinity(upper)) {
          basis.row_status[iRow] = HighsBasisStatus::UPPER;
        } else {
          basis.row_status[iRow] = HighsBasisStatus::ZERO;
        }
        if (has_simplex_basis) {
          assert(simplex_basis.nonbasicFlag_[lp.numCol_ + iRow] ==
                 NONBASIC_FLAG_TRUE);
          int move = illegal_move_value;
          if (lower == upper) {
            move = NONBASIC_MOVE_ZE;
          } else if (!highs_isInfinity(-lower)) {
            // Finite lower bound so boxed or lower
            if (!highs_isInfinity(upper)) {
              // Finite upper bound so boxed
              if (fabs(lower) < fabs(upper)) {
                move = NONBASIC_MOVE_DN;
              } else {
                move = NONBASIC_MOVE_UP;
              }
            } else {
              // Lower (since upper bound is infinite)
              move = NONBASIC_MOVE_DN;
            }
          } else if (!highs_isInfinity(upper)) {
            // Upper
            move = NONBASIC_MOVE_UP;
          } else {
            // FREE
            move = NONBASIC_MOVE_ZE;
          }
          assert(move != illegal_move_value);
          simplex_basis.nonbasicMove_[lp.numCol_ + iRow] = move;
        }
      }
    }
    if (ignore_to_ix >= ix_dim - 1) break;
  }
  return return_status;
}

void Highs::clearBasisInterface() {
  HighsModelObject& highs_model_object = hmos_[0];
  updateSimplexLpStatus(highs_model_object.ekk_instance_.simplex_lp_status_,
                        LpAction::NEW_BASIS);
}

// Get the basic variables, performing INVERT if necessary
HighsStatus Highs::getBasicVariablesInterface(int* basic_variables) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsLp& lp = lp_;
  HighsSimplexLpStatus& simplex_lp_status = ekk_instance.simplex_lp_status_;
  HighsStatus return_status = HighsStatus::OK;

  // Initialise analysis so that (even null) timing data structures
  // are set up
  ekk_instance.initialiseAnalysis();

  // Ensure that the LP (and any simplex LP) is column-wise
  setOrientation(lp_);
  if (simplex_lp_status.valid) setOrientation(ekk_instance.simplex_lp_);
  // If the simplex LP isn't initialised, scale and pass the current LP
  if (!simplex_lp_status.initialised) scaleAndPassLpToEkk(highs_model_object);

  if (!simplex_lp_status.has_basis) {
    //
    // The Ekk instance has no simplex basis, so pass the HiGHS basis
    // if it's valid, otherwise return an error for consistency with old code
    //
    // Arguable that a warning should be issued and a logical basis
    // set up
    HighsBasis& basis = highs_model_object.basis_;
    if (basis.valid_) {
      return_status = interpretCallStatus(ekk_instance.setBasis(basis),
                                          return_status, "setBasis");
      if (return_status == HighsStatus::Error) return return_status;
    } else {
      highsLogUser(
          options_.log_options, HighsLogType::ERROR,
          "getBasicVariables called without a simplex or HiGHS basis\n");
      // Arguable that a warning should be issued and a logical basis
      // set up
      //      ekk_instance.setBasis();
      return HighsStatus::Error;
    }
  }
  assert(simplex_lp_status.has_basis);

  const bool only_from_known_basis = true;
  if (ekk_instance.initialiseSimplexLpBasisAndFactor(only_from_known_basis))
    return HighsStatus::Error;
  assert(simplex_lp_status.has_invert);

  int numRow = lp.numRow_;
  int numCol = lp.numCol_;
  assert(numRow == ekk_instance.simplex_lp_.numRow_);
  for (int row = 0; row < numRow; row++) {
    int var = ekk_instance.simplex_basis_.basicIndex_[row];
    if (var < numCol) {
      basic_variables[row] = var;
    } else {
      basic_variables[row] = -(1 + var - numCol);
    }
  }
  return return_status;
}

// Solve (transposed) system involving the basis matrix

HighsStatus Highs::basisSolveInterface(const vector<double>& rhs,
                                       double* solution_vector,
                                       int* solution_num_nz,
                                       int* solution_indices, bool transpose) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HVector solve_vector;
  int numRow = ekk_instance.simplex_lp_.numRow_;
  int numCol = ekk_instance.simplex_lp_.numCol_;
  HighsScale& scale = highs_model_object.scale_;
  // Set up solve vector with suitably scaled RHS
  solve_vector.setup(numRow);
  solve_vector.clear();
  int rhs_num_nz = 0;
  if (transpose) {
    for (int row = 0; row < numRow; row++) {
      if (rhs[row]) {
        solve_vector.index[rhs_num_nz++] = row;
        double rhs_value = rhs[row];
        int col = ekk_instance.simplex_basis_.basicIndex_[row];
        if (col < numCol) {
          rhs_value *= scale.col_[col];
        } else {
          double scale_value = scale.row_[col - numCol];
          rhs_value /= scale_value;
        }
        solve_vector.array[row] = rhs_value;
      }
    }
  } else {
    for (int row = 0; row < numRow; row++) {
      if (rhs[row]) {
        solve_vector.index[rhs_num_nz++] = row;
        solve_vector.array[row] = rhs[row] * scale.row_[row];
      }
    }
  }
  solve_vector.count = rhs_num_nz;
  //
  // Note that solve_vector.count is just used to determine whether
  // hyper-sparse solves should be used. The indices of the nonzeros
  // in the solution are always accumulated. There's no switch (such
  // as setting solve_vector.count = numRow+1) to not do this.
  //
  // Get hist_dsty from analysis during simplex solve.
  double hist_dsty = 1;
  if (transpose) {
    ekk_instance.factor_.btran(solve_vector, hist_dsty);
  } else {
    ekk_instance.factor_.ftran(solve_vector, hist_dsty);
  }
  // Extract the solution
  if (solution_indices == NULL) {
    // Nonzeros in the solution not required
    if (solve_vector.count > numRow) {
      // Solution nonzeros not known
      for (int row = 0; row < numRow; row++) {
        solution_vector[row] = solve_vector.array[row];
      }
    } else {
      // Solution nonzeros are known
      for (int row = 0; row < numRow; row++) solution_vector[row] = 0;
      for (int ix = 0; ix < solve_vector.count; ix++) {
        int row = solve_vector.index[ix];
        solution_vector[row] = solve_vector.array[row];
      }
    }
  } else {
    // Nonzeros in the solution are required
    if (solve_vector.count > numRow) {
      // Solution nonzeros not known
      solution_num_nz = 0;
      for (int row = 0; row < numRow; row++) {
        solution_vector[row] = 0;
        if (solve_vector.array[row]) {
          solution_vector[row] = solve_vector.array[row];
          solution_indices[*solution_num_nz++] = row;
        }
      }
    } else {
      // Solution nonzeros are known
      for (int row = 0; row < numRow; row++) solution_vector[row] = 0;
      for (int ix = 0; ix < solve_vector.count; ix++) {
        int row = solve_vector.index[ix];
        solution_vector[row] = solve_vector.array[row];
        solution_indices[ix] = row;
      }
      *solution_num_nz = solve_vector.count;
    }
  }
  // Scale the solution
  if (transpose) {
    if (solve_vector.count > numRow) {
      // Solution nonzeros not known
      for (int row = 0; row < numRow; row++) {
        double scale_value = scale.row_[row];
        solution_vector[row] *= scale_value;
      }
    } else {
      for (int ix = 0; ix < solve_vector.count; ix++) {
        int row = solve_vector.index[ix];
        double scale_value = scale.row_[row];
        solution_vector[row] *= scale_value;
      }
    }
  } else {
    if (solve_vector.count > numRow) {
      // Solution nonzeros not known
      for (int row = 0; row < numRow; row++) {
        int col = ekk_instance.simplex_basis_.basicIndex_[row];
        if (col < numCol) {
          solution_vector[row] *= scale.col_[col];
        } else {
          double scale_value = scale.row_[col - numCol];
          solution_vector[row] /= scale_value;
        }
      }
    } else {
      for (int ix = 0; ix < solve_vector.count; ix++) {
        int row = solve_vector.index[ix];
        int col = ekk_instance.simplex_basis_.basicIndex_[row];
        if (col < numCol) {
          solution_vector[row] *= scale.col_[col];
        } else {
          double scale_value = scale.row_[col - numCol];
          solution_vector[row] /= scale_value;
        }
      }
    }
  }
  return HighsStatus::OK;
}

HighsStatus Highs::getDualRayInterface(bool& has_dual_ray,
                                       double* dual_ray_value) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsLp& lp = lp_;
  int numRow = lp.numRow_;
  has_dual_ray = ekk_instance.simplex_lp_status_.has_dual_ray;
  if (has_dual_ray && dual_ray_value != NULL) {
    vector<double> rhs;
    int iRow = ekk_instance.simplex_info_.dual_ray_row_;
    rhs.assign(numRow, 0);
    rhs[iRow] = ekk_instance.simplex_info_.dual_ray_sign_;
    int* dual_ray_num_nz = 0;
    basisSolveInterface(rhs, dual_ray_value, dual_ray_num_nz, NULL, true);
  }
  return HighsStatus::OK;
}

HighsStatus Highs::getPrimalRayInterface(bool& has_primal_ray,
                                         double* primal_ray_value) {
  HighsModelObject& highs_model_object = hmos_[0];
  HEkk& ekk_instance = highs_model_object.ekk_instance_;
  HighsLp& lp = lp_;
  int numRow = lp.numRow_;
  int numCol = lp.numCol_;
  has_primal_ray = ekk_instance.simplex_lp_status_.has_primal_ray;
  if (has_primal_ray && primal_ray_value != NULL) {
    int col = ekk_instance.simplex_info_.primal_ray_col_;
    assert(ekk_instance.simplex_basis_.nonbasicFlag_[col] ==
           NONBASIC_FLAG_TRUE);
    // Get this pivotal column
    vector<double> rhs;
    vector<double> column;
    column.assign(numRow, 0);
    rhs.assign(numRow, 0);
    // Ensure that the LP is column-wise
    setOrientation(lp_);
    int primal_ray_sign = ekk_instance.simplex_info_.primal_ray_sign_;
    if (col < numCol) {
      for (int iEl = lp.Astart_[col]; iEl < lp.Astart_[col + 1]; iEl++)
        rhs[lp.Aindex_[iEl]] = primal_ray_sign * lp.Avalue_[iEl];
    } else {
      rhs[col - numCol] = primal_ray_sign;
    }
    int* column_num_nz = 0;
    basisSolveInterface(rhs, &column[0], column_num_nz, NULL, false);
    // Now zero primal_ray_value and scatter the column according to
    // the basic variables.
    for (int iCol = 0; iCol < numCol; iCol++) primal_ray_value[iCol] = 0;
    for (int iRow = 0; iRow < numRow; iRow++) {
      int iCol = ekk_instance.simplex_basis_.basicIndex_[iRow];
      if (iCol < numCol) primal_ray_value[iCol] = column[iRow];
    }
    if (col < numCol) primal_ray_value[col] = -primal_ray_sign;
  }
  return HighsStatus::OK;
}
