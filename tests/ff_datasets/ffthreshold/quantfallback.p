% GENERATION CONFIGURATION 
%Fragment: Fluted
%Mode: SAT
%Depth: 10 | Count: 5 | Seed: 3130875450
%Domain Size: 0
%Vocabulary Size: 20 predicates.

Formula 1 
% ATTEMPTS: 1
fof(f,axiom,
    (? [X1] : (! [X2] : (~X1 = X2)))
).

Formula 2 
% ATTEMPTS: 1
fof(f,axiom,
    (? [X1] : (! [X2] : X1 = X2))
).

Formula 3 
% ATTEMPTS: 1
fof(f,axiom,
    (! [X1] : (? [X2] : X1 = X2))
).

Formula 4 
% ATTEMPTS: 1
fof(f,axiom,
    (? [X1] : (! [X2] : X1 = X2))
).

Formula 5 
% ATTEMPTS: 1
fof(f,axiom,
    (? [X1] : (! [X2] : (((~X1 = X2) & X1 = X2) => (~X1 = X2))))
).
