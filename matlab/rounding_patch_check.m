function report=rounding_patch_check(n)
%ROUNDING_PATCH_CHECK Verify patch loading and an exact binary matrix witness.
% No INTLAB required. Check at the intended BLAS thread setting.
if nargin==0, n=512; end
validateattributes(n,{'double'},{'scalar','integer','>=',2,'finite'});
root=fileparts(fileparts(mfilename('fullpath'))); addpath(fullfile(root,'build'));
assert(contains(version('-blas'),'Apple Accelerate'),'roundPatch:blas','Expected Apple Accelerate.');
initial=rounding_patch_status();
assert(initial.abi==2,'roundPatch:abi','Unexpected patch ABI.');
guard=onCleanup(@() rounding_patch_status(initial.rounding));
feature('setround',0.5);
A=zeros(n); B=zeros(n);
A(:,1)=1; A(:,2)=2^-27; B(1,:)=1; B(2,:)=2^-27;
feature('setround',-inf); D=A*B;
feature('setround',inf); U=A*B;
feature('setround',0.5);
badDown=nnz(~isfinite(D)|D>1);
badUp=nnz(~isfinite(U)|U<1+eps);
assert(badDown==0 && badUp==0,'roundPatch:enclosure','Matrix rounding failed: down=%d up=%d.',badDown,badUp);
report=struct('matlab',version,'computer',computer,'blas',version('-blas'), ...
    'threadLimit',maxNumCompThreads,'size',n,'badDown',badDown,'badUp',badUp,'patch',rounding_patch_status());
fprintf('PATCH_CHECK n=%d badDown=%d badUp=%d ABI=%d cpuFallback=%d\n', ...
    n,badDown,badUp,report.patch.abi,report.patch.cpuFallbackSelected);
end
