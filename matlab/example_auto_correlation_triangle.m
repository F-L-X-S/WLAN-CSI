% This MATLAB-script copares the results of the autocorrelation-function in
% C++ and the AutoCorr-function in MATLAB. 
% Simlply run the example_auto_corr.cc and copy x and rxx from terminal.
% Make sure to check, that Windowsize and delay for 
% the MATLAB and C++ calculation are equal.

% Window-size
window_size = 3;  

% lag (delay)
lag = 3;

% Input-signal (real-valued triangular pulse) used in correlation_test.cc 
x = [ ...
1.000000 + 1i*0.000000, 2.000000 + 1i*0.000000, 3.000000 + 1i*0.000000, 4.000000 + 1i*0.000000, 5.000000 + 1i*0.000000,  ...
5.000000 + 1i*0.000000, 4.000000 + 1i*0.000000, 3.000000 + 1i*0.000000, 2.000000 + 1i*0.000000, 1.000000 + 1i*0.000000 ...
];

% Expected Autocorrelation-result
Rxx = [ ...
0.000000 + 1i*0.000000, 0.000000 + 1i*0.000000, 0.000000 + 1i*0.000000, 4.000000 + 1i*0.000000, 14.000000 + 1i*0.000000,  ...
29.000000 + 1i*0.000000, 41.000000 + 1i*0.000000, 46.000000 + 1i*0.000000, 41.000000 + 1i*0.000000, 29.000000 + 1i*0.000000 ...
];

% Calculate and plot the autocorrelation
figure;
subplot(3,1,1); plot(real(x)); hold on;  plot(imag(x)); title('Input-signal'), legend('Real', 'Imag');grid on;
subplot(3,1,2); plot(abs(Rxx)); xlabel('Sample'); ylabel('|R_{xx}|'); title('Autocorrelation (C++)'); grid on;
subplot(3,1,3); AutoCorr(x,lag, window_size); xlabel('Sample'); ylabel('|R_{xx}|'); title('Autocorrelation (Matlab)'); grid on;