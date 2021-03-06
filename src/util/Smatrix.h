#ifndef LSMATRIX_H_
#define LSMATRIX_H_

#include <Rcpp.h>
#include "Dvector.h"

template <typename T> class SMatrix
{
public:
  DVector<uint> row_idx;
  DVector<uint> col_idx;
  DVector<T> value;

public:
  uint dim1;
  uint dim2;
  uint size;
  bool transposed; //TODO: 添加直接从numericmatrix导入数据的成员函数，标记tranposed

public:
  SMatrix()
    : dim1(0), dim2(0), size(0), transposed(false)
  {}

  SMatrix(List _list) //TODO:统一数据类型
  {
    transposed             = as<bool>(_list.attr("transposed"));
    IntegerVector dim      = _list["dim"];
    dim1                   = dim[0];
    dim2                   = dim[1];
    size                   = _list["size"];
    IntegerVector row_size = _list["row_size"];
    if (dim1 != row_size.size()) { stop("the length of input's row_size is not correct..."); }
    value.assign(_list["value"]);
    col_idx.assign(_list["col_idx"]);
    row_idx.setSize(dim1 + 1); //TODO: the last element is actually the end!!!
    row_idx.init(0);
    for (uint i = 0; i < dim1; ++i)
    {
      row_idx[i + 1] = row_idx[i] + row_size[i];
    }
  }

  void assign(List _list)
  {
    transposed             = as<bool>(_list.attr("transposed"));
    IntegerVector dim      = _list["dim"];
    dim1                   = dim[0];
    dim2                   = dim[1];
    size                   = _list["size"];
    IntegerVector row_size = _list["row_size"];
    if (dim1 != row_size.size()) { stop("the length of input's row_size is not correct..."); }
    value.assign(_list["value"]);
    col_idx.assign(_list["col_idx"]);
    row_idx.setSize(dim1 + 1); //TODO: the last element is actually the end!!!
    row_idx.init(0);
    for (uint i = 0; i < dim1; ++i)
    {
      row_idx[i + 1] = row_idx[i] + row_size[i];
    }
  }

  SMatrix(NumericMatrix m) {} //TODO;

  T operator() (uint i, uint j) const
  {
    if (size == 0) { return 0.0; } // SMatrix is empty
    uint low = row_idx[i];
    // uint high = (i == (dim1 - 1)) ? col_idx.size() : row_idx[i+1];
    uint high = row_idx[i+1]; // TEST
    if (high == low) { return 0.0; } // i row is empty
    if ((j > col_idx[high-1]) || (j < col_idx[low])) { return 0.0; } // not in range
    if (col_idx[low] == j) { return value[low]; }
    if (col_idx[high-1] == j) { return value[high-1]; }
    uint id = search_column(low, high, j);
    return (col_idx[id] == j) ? value[id] : 0.0;
  }

  uint search_column(uint low, uint high, uint column) const
  {
    uint mid;
    while (high > low)
    {
      mid = (low + high) >> 1;
      if (col_idx[mid] == column) { break; }
      if (mid < column) { low = mid + 1; }
      else { high = mid; }
    }
    return mid;
  }

  uint nrow() const { return dim1; }

  uint ncol() const { return dim2; }

  uint nvalues() const { return size; }

  List scales(IntegerVector norm_columns)
  {
    DVector<double> colSum(dim2);
    colSum.init(0.0);
    DVector<double> colSumSqr(dim2);
    colSumSqr.init(0.0);
    uint idx; double val;
    for (uint p = 0; p < size; ++p) {
      idx = col_idx[p];
      val = value[p];
      colSum[idx] += val;
      colSumSqr[idx] += val * val;
    }

    double mult_dim = (double)(dim1) * ((double)dim1 - 1);
    for (uint col = 0, i = 0; col < dim2; col ++)
    {
      if (col == (uint)norm_columns[i]) {
        colSumSqr[col] = std::sqrt(colSumSqr[col] / (dim1 - 1) - colSum[col] * colSum[col] / mult_dim);
        colSum[col] /= dim1;
        i++;
      } else {
        colSumSqr[col] = 1.0;
        colSum[col] = 0.0;
      }
    }

    for (uint p = 0; p < size; ++p) {
      idx = col_idx[p];
      value[p] -= colSum[idx];
      value[p] /= (colSumSqr[idx] + 1e-30);
    }

    return List::create(
      _["mean"] = colSum.to_rtype(),
      _["std"]  = colSumSqr.to_rtype()
    );
  }

  void normalize(List scale)
  {
    DVector<double> colmean;
    colmean.assign(as<NumericVector>(scale["mean"]));
    DVector<double> colstd;
    colstd.assign(as<NumericVector>(scale["std"]));
    if (dim2 != colmean.size() || dim2 != colstd.size()) {
      stop("the length of scale:mean or scale:std is not equal");
    }
    for (uint p = 0; p < value.size(); ++p)
    {
      int i = col_idx[p];
      if (colstd[i] != 0) {
        value[p] = (value[p] - colmean[i]) / colstd[i];
      }
    }
  }

  void transpose(SMatrix<T> &mt)
  {
    mt.dim1 = dim2;
    mt.dim2 = dim1;
    mt.size = size;
    mt.transposed = true;
    mt.row_idx.setSize(mt.dim1 + 1);
    mt.row_idx.init(0);
    mt.col_idx.setSize(mt.size);
    mt.value.setSize(mt.size);

    DVector<uint>row_p;
    row_p.assign(row_idx);
    T* value_p = mt.value.begin();
    uint* row_idx_p = mt.row_idx.begin();
    uint* col_idx_p = mt.col_idx.begin();

    for (uint i = 0; i < dim2; i++) {
      row_idx_p ++;
      *row_idx_p = *(row_idx_p - 1);
      for (uint j = 0; j < dim1; j++) {
        uint &value_idx = row_p[j];
        if (col_idx[value_idx] == i) {
          *value_p = value[value_idx];  value_p ++;
          *col_idx_p = j; col_idx_p ++;
          *row_idx_p += 1;
          if (value_idx < row_p[j+1] - 1) value_idx ++;
        }
      }
    }
  }

public:
  class Iterator;

};

template<typename T>
class SMatrix<T>::Iterator
{
protected:
  const SMatrix<T>& matrix;
  uint begin;
  uint end;
  uint pointer;
  uint row;
public:
  uint index;
  T value;

public:
  Iterator(const SMatrix<T>& _matrix, uint row_no)
    : matrix(_matrix)
  {
    begin   = matrix.row_idx[row_no];
    // end     = (row_no == (matrix.dim1 - 1)) ? matrix.size : matrix.row_idx[row_no+1];
    end     = matrix.row_idx[row_no+1];
    pointer = begin;
    row     = row_no;
    index   = matrix.col_idx[pointer];
    value   = matrix.value[pointer];
  }

  Iterator(const SMatrix<T>& _matrix)
    : matrix(_matrix), begin(0), end(_matrix.size), pointer(0), row(0), index(_matrix.col_idx[0]), value(_matrix.value[0])
  {}

  ~Iterator() {}

  bool is_end() { return pointer == end; }

  void operator ++()
  {
    pointer += 1;
    index = matrix.col_idx[pointer];
    value = matrix.value[pointer];
  }

  void next()
  { if (row == (matrix.dim1 - 1)) { return; }
    row   += 1;
    begin = pointer = matrix.row_idx[row];
    // end   = (row == (matrix.dim1 - 1)) ? matrix.size : matrix.row_idx[row+1];
    end   = matrix.row_idx[row+1];
    index = matrix.col_idx[pointer];
    value = matrix.value[pointer];
  }

  void reset()
  {
    pointer = begin;
    index   = matrix.col_idx[pointer];
    value   = matrix.value[pointer];
  }

  void set_row(uint row_no)
  {
    begin   = matrix.row_idx[row_no];
    end     = matrix.row_idx[row_no+1];
    pointer = begin;
    row     = row_no;
    index   = matrix.col_idx[pointer];
    value   = matrix.value[pointer];
  }
};


#endif
