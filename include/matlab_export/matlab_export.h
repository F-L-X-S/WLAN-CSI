#ifndef MATLAB_EXPORT_H
#define MATLAB_EXPORT_H
#include <complex>

/**
 * @brief Function to export a vector of complex numbers to a MATLAB file. The vector is
 * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
 * 
 * @param x 
 * @param varname 
 * @param outfile 
 */
 void MatlabExport(const std::vector<std::complex<float>>& x, const std::string& varname, const std::string& outfile);

/**
 * @brief Function to export a vector of real numbers to a MATLAB file. The vector is
 * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
 * 
 * @param x 
 * @param varname 
 * @param outfile 
 */
void MatlabExport(const std::vector<float>& x, const std::string& varname, const std::string& outfile);

#endif // MATLAB_EXPORT_H