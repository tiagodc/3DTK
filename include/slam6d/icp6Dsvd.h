/** @file 
 *  @brief Definition of the ICP error function minimization
 *  @author Kai Lingemann. Inst. of CS, University of Osnabrueck, Germany.
 *  @author Andreas Nuechter. Jacobs University Bremen gGmbH, Germany
 */

#ifndef __ICP6DSVD_H__
#define __ICP6DSVD_H__

#include "icp6Dminimizer.h"

/**
 * @brief Implementation of the ICP error function minimization via SVD
 */
class icp6D_SVD : public icp6Dminimizer
{
public:
  /** 
   * Constructor 
   */
  icp6D_SVD(bool quiet = false) : icp6Dminimizer(quiet) {};
  
  /** 
   * Destructor 
   */
  virtual ~icp6D_SVD() {};                                

  double Align(const vector<PtPair>& Pairs,
			double *alignxf,
			const double centroid_m[3],
			const double centroid_d[3]);  

  double Align_Parallel(const int openmp_num_threads, 
				    const unsigned int n[OPENMP_NUM_THREADS],
				    const double sum[OPENMP_NUM_THREADS], 
				    const double centroid_m[OPENMP_NUM_THREADS][3],
				    const double centroid_d[OPENMP_NUM_THREADS][3], 
				    const double Si[OPENMP_NUM_THREADS][9],
				    double *alignxf);
  
  inline int getAlgorithmID() { return 2; }; 
};

#endif
