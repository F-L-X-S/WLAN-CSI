/**
 * @file matlab_export.h
 * @author Felix Schuelke (flxscode@gmail.com)
 * 
 * @brief This file contains the definition of the MatlabExport class, 
 * which is used to export vector-formatted data from C++ to a MATLAB file.
 * 
 * @version 0.1
 * @date 2025-05-20
 * 
 * 
 */

#ifndef MATLAB_EXPORT_H
#define MATLAB_EXPORT_H
#include <complex>
#include <fstream>

class MatlabExport {
    public: 
    /**
     * @brief Intialize a MATLAB file. The constructor clears the file.
     * 
     * @param outfile 
     */
    MatlabExport(const std::string& outfile);


    /**
     * @brief Destroy the MatlabExport object and close the file.
     * 
     */
    ~MatlabExport();

    /**
     * @brief Function to export a vector of complex numbers to a MATLAB file. The vector is
     * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
     * 
     * @param x 
     * @param varname 
     */
    MatlabExport& Add(const std::vector<std::complex<float>>& x, const std::string& varname);

    /**
     * @brief Function to export a vector of real numbers to a MATLAB file. The vector is
     * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
     * 
     * @param x 
     * @param varname 
     */
    MatlabExport& Add(const std::vector<float>& x, const std::string& varname);

    /**
     * @brief Function to Export a string-formatted Matlab-command or script to to specified output file.
     * 
     * @param commandline 
     */
    MatlabExport& Add(const std::string& commandline);

    private:
        std::ofstream file_;
};
#endif // MATLAB_EXPORT_H