#include <matlab_export/matlab_export.h>
#include <fstream>
#include <iostream>


 /**
  * @brief Function to intialize a MATLAB file. The function clears the file.
  * 
  * @param outfile 
  */
 void InitMatlabExport(const std::string& outfile) {
    std::ofstream file(outfile, std::ios::trunc);
    if (!file) {
        std::cerr << "Error: File stream not ready for writing: " << outfile << std::endl;
        return;
    }
    file << "% Auto-generated MATLAB script\n";
    file << "clear;\n";
    file.close();
}

 /**
  * @brief Function to export a vector of complex numbers to a MATLAB file. The vector is
  * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
  * 
  * @param x 
  * @param varname 
  * @param outfile 
  */
void MatlabExport(const std::vector<std::complex<float>>& x, const std::string& varname, const std::string& outfile) {
    std::ofstream file(outfile, std::ios::app); 
    if (!file) {
        std::cerr << "Error: File stream not ready for writing: " << outfile << std::endl;
        return;
    }
    file << varname << " = [ ...\n";
    for (size_t i = 0; i < x.size(); ++i) {
        float re = x[i].real();
        float im = x[i].imag();
        file << re << " + 1i*" << im;
        if (i < x.size() - 1)
            file << ", ";
        if ((i + 1) % 5 == 0)
            file << " ...\n";
    }
    file << "];" << std::endl;

    file.close();
}

 /**
  * @brief Function to export a vector of real numbers to a MATLAB file. The vector is
  * appended to the end of the file. The function takes the vector, a variable name, and the outputfile
  * 
  * @param x 
  * @param varname 
  * @param outfile 
  */
void MatlabExport(const std::vector<float>& x, const std::string& varname, const std::string& outfile) {
    std::ofstream file(outfile, std::ios::app); 
    if (!file) {
        std::cerr << "Error: File stream not ready for writing: " << outfile << std::endl;
        return;
    }
    file << varname << " = [ ...\n";
    for (size_t i = 0; i < x.size(); ++i) {
        file << x[i];
        if (i < x.size() - 1)
            file << ", ";
        if ((i + 1) % 5 == 0)
            file << " ...\n";
    }
    file << "];" << std::endl;

    file.close();
}


/**
 * @brief Function to Export a string-formatted Matlab-command or script to to specified output file.
 * 
 * @param commandline 
 * @param outfile 
 */
void MatlabExport(const std::string& commandline, const std::string& outfile){
    std::ofstream file(outfile, std::ios::app); 
    if (!file) {
        std::cerr << "Error: File stream not ready for writing: " << outfile << std::endl;
        return;
    }
    file << commandline << std::endl;
    file.close();
}