/**
 * @file matlab_export.cc
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

#include <matlab_export/matlab_export.h>
#include <iostream>


 /**
  * @brief Function to initialize a MATLAB file. The function clears the file.
  * 
  * @param outfile 
  */
 MatlabExport::MatlabExport(const std::string& outfile): 
    file_(outfile, std::ios::trunc)
    {
    if (!file_) {
        std::cerr << "Error: File stream not ready for writing: " << outfile << std::endl;
        return;
    }
    file_ << "% Auto-generated MATLAB script\n";
    file_ << "clear;\n";
}


    /**
     * @brief Destroy the MatlabExport object and close the file.
     * 
     */
 MatlabExport::~MatlabExport() {
    file_.close();
}

 /**
  * @brief Function to export a vector of complex numbers to a MATLAB file. The vector is
  * appended to the end of the file. Tha variable name is stored in the class instance. 
  * 
  * @param x 
  * @param varname  
  */
 MatlabExport& MatlabExport::Add(const std::vector<std::complex<float>>& x, const std::string& varname) {
    // Add data to M-File
    file_ << varname << " = [ ...\n";
    for (size_t i = 0; i < x.size(); ++i) {
        float re = x[i].real();
        float im = x[i].imag();
        file_ << re << " + 1i*" << im;
        if (i < x.size() - 1)
            file_ << ", ";
        if ((i + 1) % 5 == 0)
            file_ << " ...\n";
    }
    file_ << "];" << std::endl;

    // Store the variable name
    varnames_.push_back(varname);

    return *this;
}

 /**
  * @brief Function to export a vector of real numbers to a MATLAB file. The vector is
  * appended to the end of the file. Tha variable name is stored in the class instance. 
  * 
  * @param x 
  * @param varname 
  */
 MatlabExport& MatlabExport::Add(const std::vector<float>& x, const std::string& varname) {
    // Add data to M-File
    file_ << varname << " = [ ...\n";
    for (size_t i = 0; i < x.size(); ++i) {
        file_ << x[i];
        if (i < x.size() - 1)
            file_ << ", ";
        if ((i + 1) % 5 == 0)
            file_ << " ...\n";
    }
    file_ << "];" << std::endl;

    // Store the variable name
    varnames_.push_back(varname);

    return *this;
}


/**
 * @brief Function to Export a string-formatted Matlab-command or script to to specified output file.
 * 
 * @param commandline 
 */
MatlabExport& MatlabExport::Add(const std::string& commandline){
    file_ << commandline << std::endl;
    return *this;
}

/**
 * @brief Function to get the variable names stored in the class instance.
 * 
 * @return std::vector<std::string> 
 */
std::vector<std::string> MatlabExport::GetVarNames() const {
    return varnames_;
}