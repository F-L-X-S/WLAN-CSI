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
  * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
  * 
  * @param x 
  * @param varname  
  */
 MatlabExport& MatlabExport::Add(const std::vector<std::complex<float>>& x, const std::string& varname) {
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
    return *this;
}

 /**
  * @brief Function to export a vector of real numbers to a MATLAB file. The vector is
  * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
  * 
  * @param x 
  * @param varname 
  */
 MatlabExport& MatlabExport::Add(const std::vector<float>& x, const std::string& varname) {
    file_ << varname << " = [ ...\n";
    for (size_t i = 0; i < x.size(); ++i) {
        file_ << x[i];
        if (i < x.size() - 1)
            file_ << ", ";
        if ((i + 1) % 5 == 0)
            file_ << " ...\n";
    }
    file_ << "];" << std::endl;
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