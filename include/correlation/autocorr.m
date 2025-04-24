% MATLAB Implementation of the liquid-dsp autocorr_cccf functions
% Note that matlab indicates arrays starting at 1 (C/C++ starts at zero)

function rxx_lag = AutoCorr(x, lag, window_size)   
    % intialize buffer for correlation of specific lag
    rxx_lag = zeros(1, length(x));
    
    % initialize buffertwice the window-size (for shifting)
    buffer = zeros(1, 2*window_size);

    for n = 1 : length(x)
        % leftshift the buffer with the n-th value of x-vector
        buffer = [buffer(2:end), x(n)];  

        % autocorrelation of buffer  
        rxx_full = xcorr(buffer, 'none');
        rxx = rxx_full(length(buffer)+lag);
    
        % Get autocorrelation-value for specified lag
        rxx_lag(n) = rxx;
    
        % energy in corr-window (right half of buffer)
        energy = sum(abs(buffer(window_size+1:end)).^2);
    
        % print to termnal
        fprintf('%s: Energy: %.2f | Rxx: %.2f\n', num2str(n), energy, rxx);
    end
    
    % Plot
    plot(abs(rxx_lag));
    title('Autocorrelation');
    xlabel('Sample');
    ylabel('|R_{xx}|');
end

% real-valued triangular signal
x = [ ...
1.000000 + 1i*0.000000, 2.000000 + 1i*0.000000, 3.000000 + 1i*0.000000, 4.000000 + 1i*0.000000, 5.000000 + 1i*0.000000,  ...
5.000000 + 1i*0.000000, 4.000000 + 1i*0.000000, 3.000000 + 1i*0.000000, 2.000000 + 1i*0.000000, 1.000000 + 1i*0.000000 ...
];

% Window-size
window_size = 3;  

% lag (delay)
lag = 3;

% Calculate and plot the autocorrelation
AutoCorr(x,lag, window_size);