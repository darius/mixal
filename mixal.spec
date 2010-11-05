Name: mixal
Version: 1.11
Release: 1
Source0: %{name}-%{version}.tar.gz
Copyright: distributable
Group: Development/Languages
Summary: load-and-go assembler for Donald Knuth's MIX language
BuildRoot: %{_tmppath}/%{name}-root
URL: http://www.catb.org/~esr/mixal/
#Keywords: retrocomputing, Knuth, assembler
#Destinations: mailto:vlm@debian.org

%description
mixal is an assembler and interpreter for Donald Knuth's mythical MIX
computer, defined in: Donald Knuth, "The Art of Computer Programming",
Vol. 1: Fundamental Algorithms_.  Addison-Wesley, 1973 (2nd ed.)

%prep
%setup -q

%build
make %{?_smp_mflags} mixal mixal.1

%install
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"
mkdir -p "$RPM_BUILD_ROOT"%{_bindir}
mkdir -p "$RPM_BUILD_ROOT"%{_mandir}/man1/
cp mixal "$RPM_BUILD_ROOT"%{_bindir}
cp mixal.1 "$RPM_BUILD_ROOT"%{_mandir}/man1/

%clean
[ "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf "$RPM_BUILD_ROOT"

%files
%doc README MIX.DOC NOTES
%{_bindir}/mixal
%{_mandir}/man1/mixal.1*

%changelog
* Wed Oct 20 2010 Eric S. Raymond <esr@golux.thyrsus.com> 1.11-1
- Much of README merged to the manual page and COPYING.
- Custom license regularized to the (isomorphic) zlib/libpng license.

* Fri Jan 16 2004 Eric S. Raymond <esr@golux.thyrsus.com> 1.10-1
- Addison-Wesley and Donald Knuth have specifically granted
  permission for the MIX source code and docs to be distributed in
  connection with open-source MIX implementations.

* Mon Dec 29 2003 Eric S. Raymond <esr@snark.thyrsus.com> 1.09-1
- Non-root users can now build the RPMs.
- Debian documentation from 1.08 merged and lifted to XML.
- Build process now copes with stdin and stdout not being constants.

* Tue Feb 2 1999 Antti-Juhani Kaijanaho <ajk@debian.org> 1.08-1
- Disk and console devices are now implemented.

* Fri Dec 18 1998 Eric S. Raymond <esr@snark.thyrsus.com> 1.07-1
- Initial port to Unix systems.
