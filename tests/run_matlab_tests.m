function run_matlab_tests
root=fileparts(fileparts(mfilename('fullpath')));
addpath(fullfile(root,'matlab'),fullfile(root,'build'),fullfile(root,'tests'));
resultFile=fullfile(root,'build','matlab-results.json');
if isfile(resultFile), delete(resultFile); end
reports=cell(1,5); sizes=[287 288 512 540 1024];
for i=1:numel(sizes), reports{i}=rounding_patch_check(sizes(i)); end
intlabRoot=getenv('INTLAB_ROOT');
testedIntlab=false;
extended=[];
if ~isempty(intlabRoot)
    assert(isfile(fullfile(intlabRoot,'startintlab.m')),'Invalid INTLAB_ROOT.');
    % Explicit opt-in: startintlab can write its installation cache.
    old=pwd; restore=onCleanup(@() cd(old)); addpath(intlabRoot); invokeStartup();
    addpath(fullfile(root,'tests')); cd(old);
    for n=[288 512 540 1024]
        setround(0); output=evalc('testmm(n)');
        assert(~contains(output,'error!'),'testmm(%d) failed: %s',n,output);
        fprintf('TESTMM n=%d passed=1\n',n);
    end
    % Radius alone is insufficient; explicitly check the exact enclosure.
    n=512; A=zeros(n); B=zeros(n);
    A(:,1)=1; A(:,2)=2^-27; B(1,:)=1; B(2,:)=2^-27;
    C=intval(A)*B; lo=inf(C); hi=sup(C);
    assert(all(isfinite(lo(:)) & isfinite(hi(:)) & lo(:)<=1 & hi(:)>=1+eps));
    testedIntlab=true;
    extended=check_intlab_enclosures();
end
result=struct('checks',{reports},'intlabTested',testedIntlab,'enclosures',extended);
fid=fopen(resultFile,'w'); assert(fid~=-1);
cleanup=onCleanup(@() fclose(fid)); fprintf(fid,'%s\n',jsonencode(result));
fprintf('MATLAB_TESTS_COMPLETE intlab=%d\n',testedIntlab);
end
function invokeStartup
startintlab;
end
