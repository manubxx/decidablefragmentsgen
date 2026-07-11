% GENERATION CONFIGURATION 
%Fragment: Fluted
%Mode: UNSAT
%Depth: 12 | Count: 5 | Seed: 3540000290
%Domain Size: 0
%Vocabulary Size: 5 predicates.

Formula 1 
% ATTEMPTS: 1
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : (X1 = X2 | ((X1 = X2 => (~X1 = X2)) => (~X1 = X2))))) & (~(! [X1] : (? [X2] : (X1 = X2 | ((X1 = X2 => (~X1 = X2)) => (~X1 = X2)))))))
).

Formula 2 
% ATTEMPTS: 1
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : ((~(~((~X1 = X2) => (~(~(~X1 = X2)))))) => ((X1 = X2 => (X1 = X2 => (~X1 = X2))) | (~(~X1 = X2)))))) & (~(! [X1] : (? [X2] : ((~(~((~X1 = X2) => (~(~(~X1 = X2)))))) => ((X1 = X2 => (X1 = X2 => (~X1 = X2))) | (~(~X1 = X2))))))))
).

Formula 3 
% ATTEMPTS: 1
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : ((~X1 = X2) | X1 = X2))) & (~(! [X1] : (? [X2] : ((~X1 = X2) | X1 = X2)))))
).

Formula 4 
% ATTEMPTS: 1
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : (X1 = X2 | X1 = X2))) & (~(! [X1] : (? [X2] : (X1 = X2 | X1 = X2)))))
).

Formula 5 
% ATTEMPTS: 1
fof(f,negated_conjecture,
    ((? [X1] : (! [X2] : ((~(X1 = X2 => X1 = X2)) | X1 = X2))) & (~(? [X1] : (! [X2] : ((~(X1 = X2 => X1 = X2)) | X1 = X2)))))
).
