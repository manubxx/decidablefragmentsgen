% GENERATION CONFIGURATION 
%Fragment: Fluted
%Mode: UNSAT
%Depth: 2 | Count: 5 | Seed: 1247896149
%Domain Size: 0
%Vocabulary Size: 5 predicates.
% Formula 1
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : b2(X1,X2))) & (~(! [X1] : (? [X2] : b2(X1,X2)))))
).

% Formula 2
fof(f,negated_conjecture,
    (((? [X1] : (? [X2] : b5(X1,X2))) & (? [X1] : (? [X2] : b2(X1,X2)))) & (~((? [X1] : (? [X2] : b5(X1,X2))) & (? [X1] : (? [X2] : b2(X1,X2))))))
).

% Formula 3
fof(f,negated_conjecture,
    (((! [X1] : (! [X2] : b2(X1,X2))) | (? [X1] : (! [X2] : b3(X1,X2)))) & (~((! [X1] : (! [X2] : b2(X1,X2))) | (? [X1] : (! [X2] : b3(X1,X2))))))
).

% Formula 4
fof(f,negated_conjecture,
    ((! [X1] : (? [X2] : b3(X1,X2))) & (~(! [X1] : (? [X2] : b3(X1,X2)))))
).

% Formula 5
fof(f,negated_conjecture,
    (((! [X1] : (! [X2] : b2(X1,X2))) => (! [X1] : (! [X2] : b3(X1,X2)))) & (~((! [X1] : (! [X2] : b2(X1,X2))) => (! [X1] : (! [X2] : b3(X1,X2))))))
).

