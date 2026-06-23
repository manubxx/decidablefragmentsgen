% GENERATION CONFIGURATION 
%Fragment: Fluted
%Mode: UNSAT
%Depth: 4 | Count: 5 | Seed: 298592890
%Domain Size: 0
%Vocabulary Size: 5 predicates.
% Formula 1
fof(f,negated_conjecture,
    ((? [X1] : (! [X2] : ((? [X3] : c2(X1,X2,X3)) & (! [X3] : c4(X1,X2,X3))))) & (~(? [X1] : (! [X2] : ((? [X3] : c2(X1,X2,X3)) & (! [X3] : c4(X1,X2,X3)))))))
).

% Formula 2
fof(f,negated_conjecture,
    (((! [X1] : (~(! [X2] : (? [X3] : c1(X1,X2,X3))))) | (? [X1] : ((? [X2] : (! [X3] : c3(X1,X2,X3))) => (! [X2] : (? [X3] : c1(X1,X2,X3)))))) & (~((! [X1] : (~(! [X2] : (? [X3] : c1(X1,X2,X3))))) | (? [X1] : ((? [X2] : (! [X3] : c3(X1,X2,X3))) => (! [X2] : (? [X3] : c1(X1,X2,X3))))))))
).

% Formula 3
fof(f,negated_conjecture,
    ((? [X1] : (? [X2] : ((? [X3] : c4(X1,X2,X3)) | (? [X3] : c4(X1,X2,X3))))) & (~(? [X1] : (? [X2] : ((? [X3] : c4(X1,X2,X3)) | (? [X3] : c4(X1,X2,X3)))))))
).

% Formula 4
fof(f,negated_conjecture,
    (((~((? [X1] : (! [X2] : (! [X3] : c4(X1,X2,X3)))) => (? [X1] : (! [X2] : (! [X3] : c1(X1,X2,X3)))))) & (~((? [X1] : (! [X2] : (? [X3] : c4(X1,X2,X3)))) & (? [X1] : (! [X2] : (? [X3] : c5(X1,X2,X3))))))) & (~((~((? [X1] : (! [X2] : (! [X3] : c4(X1,X2,X3)))) => (? [X1] : (! [X2] : (! [X3] : c1(X1,X2,X3)))))) & (~((? [X1] : (! [X2] : (? [X3] : c4(X1,X2,X3)))) & (? [X1] : (! [X2] : (? [X3] : c5(X1,X2,X3)))))))))
).

% Formula 5
fof(f,negated_conjecture,
    ((? [X1] : ((~(! [X2] : (! [X3] : c3(X1,X2,X3)))) & ((! [X2] : (! [X3] : c3(X1,X2,X3))) => (? [X2] : (? [X3] : c3(X1,X2,X3)))))) & (~(? [X1] : ((~(! [X2] : (! [X3] : c3(X1,X2,X3)))) & ((! [X2] : (! [X3] : c3(X1,X2,X3))) => (? [X2] : (? [X3] : c3(X1,X2,X3))))))))
).

