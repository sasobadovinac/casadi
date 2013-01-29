/*
 *    This file is part of CasADi.
 *
 *    CasADi -- A symbolic framework for dynamic optimization.
 *    Copyright (C) 2010 by Joel Andersson, Moritz Diehl, K.U.Leuven. All rights reserved.
 *
 *    CasADi is free software; you can redistribute it and/or
 *    modify it under the terms of the GNU Lesser General Public
 *    License as published by the Free Software Foundation; either
 *    version 3 of the License, or (at your option) any later version.
 *
 *    CasADi is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *    Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public
 *    License along with CasADi; if not, write to the Free Software
 *    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef SX_FUNCTION_INTERNAL_HPP
#define SX_FUNCTION_INTERNAL_HPP

#include "sx_function.hpp"
#include "x_function_internal.hpp"

#ifdef WITH_LLVM
// Some forward declarations
namespace llvm{
  class Module;
  class Function;
} // namespace llvm
#endif // WITH_LLVM

#ifdef WITH_OPENCL
#ifdef __APPLE__
#include <OpenCL/opencl.h>
#else
#include <CL/cl.h>
#endif
#endif // WITH_OPENCL

namespace CasADi{
#ifdef WITH_OPENCL
  /** \brief Singleton for the sparsity propagation kernel
      TODO: Move to a separate file and make non sparsity pattern specific
      \author Joel Andersson
      \date 2013
  */
  class SparsityPropagationKernel{
  public:
    // Default constructor
    SparsityPropagationKernel();
    
    // Destructor
    ~SparsityPropagationKernel();

    // Copy constructor and equality operator (not implemented, declared to prevent use of the default ones)
    SparsityPropagationKernel(const SparsityPropagationKernel& sparsityPropagationKernel);
    SparsityPropagationKernel& operator=(const SparsityPropagationKernel& sparsityPropagationKernel);

    // Data members (all public)
    cl_device_id device_id;
    cl_context context;
    cl_command_queue command_queue;
    cl_platform_id platform_id;
    cl_uint ret_num_devices;
    cl_uint ret_num_platforms;
  };
#endif // WITH_OPENCL

/** \brief  Internal node class for SXFunction
  A regular user should never work with any Node class. Use SXFunction directly.
  \author Joel Andersson 
  \date 2010
*/
class SXFunctionInternal : public XFunctionInternal<SXFunction,SXFunctionInternal,Matrix<SX>,SXNode>{
  friend class SXFunction;
  
  protected:
    /** \brief  Constructor (only to be called from SXFunction, therefore protected) */
    SXFunctionInternal(const std::vector<Matrix<SX> >& inputv, const std::vector<Matrix<SX> >& outputv);

  public:

  /** \brief  Make a deep copy */
  virtual SXFunctionInternal* clone() const;
    
  /** \brief  Destructor */
  virtual ~SXFunctionInternal();

  /** \brief  Evaluate the function numerically */
  virtual void evaluate(int nfdir, int nadir);

  /** \brief  Helper class to be plugged into evaluateGen when working with a value known only at runtime */
  struct int_runtime{
    const int value;
    int_runtime(int v) : value(v){}
  };
  
  /** \brief  Helper class to be plugged into evaluateGen when working with a value known already at compiletime */
  template<int v>
  struct int_compiletime{
    static const int value = v;
  };
  
  /** \brief  Evaluate the function numerically, first argument generic */
  template<typename T1>
  void evaluateGen1(T1 nfdir_c, int nadir);
  
  /** \brief  Evaluate the function numerically, both arguments generic */
  template<typename T1, typename T2>
  void evaluateGen(T1 nfdir_c, T2 nadir_c);
  
  /** \brief  evaluate symbolically while also propagating directional derivatives */
  virtual void evalSX(const std::vector<SXMatrix>& arg, std::vector<SXMatrix>& res, 
                      const std::vector<std::vector<SXMatrix> >& fseed, std::vector<std::vector<SXMatrix> >& fsens, 
                      const std::vector<std::vector<SXMatrix> >& aseed, std::vector<std::vector<SXMatrix> >& asens,
                      bool output_given);
                          
  /** \brief  Check if smooth */
  bool isSmooth() const;

  /** \brief  Print the algorithm */
  virtual void print(std::ostream &stream) const;

  /** \brief Hessian (forward over adjoint) via source code transformation */
  SXMatrix hess(int iind=0, int oind=0);
  
  /** \brief  DATA MEMBERS */
  
  /** \brief  An elemenent of the algorithm, namely a binary operation */
  typedef SXAlgEl AlgEl;
  
  /** \brief  An elemenent of the tape */
  template<typename T>
  struct TapeEl{
    T d[2];
  };
  
  /** \brief  all binary nodes of the tree in the order of execution */
  std::vector<AlgEl> algorithm_;

  /** \brief  Working vector for numeric calculation */
  std::vector<double> work_;
  std::vector<TapeEl<double> > pdwork_;

  /// work vector for symbolic calculations (allocated first time)
  std::vector<SX> s_work_;
  std::vector<SX> free_vars_;
  
  /// The expressions corresponding to each binary operation
  std::vector<SX> operations_;
  
  /// The expressions corresponding to each constant
  std::vector<SX> constants_;
  
  /** \brief  Initialize */
  virtual void init();

  /** \brief  Update the number of sensitivity directions during or after initialization */
  virtual void updateNumSens(bool recursive);

  /** \brief  Print to a c file */
  static void printVector(std::ostream &cfile, const std::string& name, const std::vector<int>& v);

  /** \brief  Print to a c file */
  void generateCode(const std::string& filename);
      
  /** \brief Clear the function from its symbolic representation, to free up memory, no symbolic evaluations are possible after this */
  void clearSymbolic();
  
  /// Propagate a sparsity pattern through the algorithm
  virtual void spEvaluate(bool fwd);

  /// Is the class able to propate seeds through the algorithm?
  virtual bool spCanEvaluate(bool fwd){ return true;}

  /// Reset the sparsity propagation
  virtual void spInit(bool fwd);
  
  /// Get jacobian of all nonzero outputs with respect to all nonzero inputs
  virtual FX getFullJacobian();

  /// With just-in-time compilation
  bool just_in_time_;
  
#ifdef WITH_LLVM
  llvm::Module *jit_module_;
  llvm::Function *jit_function_;

  // Function pointer type to the JIT evaluate function
  typedef void (*evaluateFcn)(double**,double**);
  
  // JIT function
  evaluateFcn jitfcn_;

  // References to input and output nonzeros
  std::vector<double*> input_ref_, output_ref_;
#endif // WITH_LLVM
  
#ifdef WITH_OPENCL
  // OpenCL memory
  std::vector<cl_mem> input_memobj_, output_memobj_;
  cl_kernel fwd_kernel_, adj_kernel_;
  cl_program program_;

  // Initialize OpenCL
  void initOpenCL();

  // Propagate sparsity using OpenCL
  void spEvaluateOpenCL(bool fwd);

  // Free OpenCL memory
  void freeOpenCL();

  static SparsityPropagationKernel sparsity_propagation_kernel_;
#endif // WITH_OPENCL

};


} // namespace CasADi

#endif // SX_FUNCTION_INTERNAL_HPP
