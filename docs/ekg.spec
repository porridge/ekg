# $Revision$, $Date$
%define        	snapshot	20011201
Summary:	A client compatible with Gadu-Gadu 	
Summary(pl):	Eksperymentalny Klient Gadu-Gadu 	
Name:		ekg		
Version:	0.9.0.%{snapshot}
Release:	1
License:	GPL
Group:		Networking/Utilities
Group(de):	Netzwerkwesen/Werkzeuge
Group(es):	Red/Utilitarios
Group(pl):	Sieciowe/Narzêdzia
Group(pt_BR):	Rede/Utilitários
Source0:	http://dev.null.pl/ekg/%{name}-%{snapshot}.tar.gz
URL:		http://dev.null.pl/ekg/	
BuildRequires:	ncurses-devel
BuildRequires:	readline-devel
BuildRoot:	%{tmpdir}/%{name}-%{version}.%{snapshot}-root-%(id -u -n)

%description
A client compatible with Gadu-Gadu.

%description -l pl
Eksperymentalny Klient Gadu-Gadu.

%package -n libgg
Summary:	libgg Library
Summary(pl):	Biblioteka libgg 
Group:		Libraries
Group(pl):	Biblioteki 

%description -n libgg 
libgg is intended to make it easy to add Gadu-Gadu communication support 
to your software.

%description -n libgg -l pl
libgg umo¿liwia ³atwe dodanie do ró¿nych aplikacji komunikacji bazuj±cej 
na protokole Gadu-Gadu.

%package -n libgg-devel
Summary:	libgg Library Development 
Summary(pl):	Czê¶æ dla programistów biblioteki libgg
Group:		Development/Libraries
Group(pl):	Programowanie/Biblioteki 
Requires:	libgg

%description -n libgg-devel
The libgg-devel package contains the header files and some documentation 
needed to develop application with libgg.

%description -n libgg-devel -l pl
Pakiet libgg-devel zawiera pliki nag³ówkowe i dokumentacjê, potrzebne 
do kompilowania aplikacji korzystaj±cych z libgg.

%package -n libgg-static
Summary:	Static libgg Library 
Summary(pl):	Statyczna biblioteka libgg
Group:		Development/Libraries
Group(pl):	Programowanie/Biblioteki 
Requires:	libgg-devel 

%description -n libgg-static
Static libgg library.

%description -n libgg-static -l pl
Statyczna biblioteka libgg.
 
%prep
%setup -q -n %{name}-%{snapshot} 

%build
./configure \
	%{?!debug:--without-debug}
make

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_bindir}
install -d $RPM_BUILD_ROOT%{_includedir}
install -d $RPM_BUILD_ROOT%{_libdir}
install -d $RPM_BUILD_ROOT%{_datadir}/ekg

install src/ekg $RPM_BUILD_ROOT%{_bindir}
install lib/libgg.so.* $RPM_BUILD_ROOT%{_libdir}
install lib/libgg.a $RPM_BUILD_ROOT%{_libdir}
ln -s %{_libdir}/libgg.so.0.9.0 $RPM_BUILD_ROOT%{_libdir}/libgg.so
install lib/libgg.h $RPM_BUILD_ROOT%{_includedir}

install themes/*.theme $RPM_BUILD_ROOT%{_datadir}/ekg

gzip -9nf ChangeLog docs/* 

%post -n libgg -p /sbin/ldconfig 
%postun -n libgg -p /sbin/ldconfig 

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%doc ChangeLog.gz docs/README.gz
%doc docs/7thguard.txt.gz docs/themes.txt.gz 
%doc docs/ekl.pl.gz docs/ekg.man.gz 
%attr(755,root,root) %{_bindir}/* 
%{_datadir}/ekg
%files -n libgg
%defattr(644,root,root,755)
%attr(644,root,root) %{_libdir}/libgg.so.*

%files -n libgg-devel
%defattr(644,root,root,755)
%attr(644,root,root) %{_libdir}/libgg.so
%attr(644,root,root) %{_includedir}/* 
%doc docs/protocol.txt.gz
%doc docs/api.txt.gz
%doc docs/7thguard.txt.gz
%doc ChangeLog.gz README.gz

%files -n libgg-static
%defattr(644,root,root,755)
%attr(644,root,root) %{_libdir}/libgg.a

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
%changelog
* %{date} PLD Team <pld-list@pld.org.pl>
All persons listed below can be reached at <cvs_login>@pld.org.pl

$Log$
Revision 1.2  2001/12/02 14:29:13  speedy
pld'owskie spec dla ekg. zawsze nieaktualne... ;>

Revision 1.16  2001/12/02 14:27:01  speedy
- updated to 20011201.

Revision 1.15  2001/11/21 23:51:31  undefine
- added %{_datadir}/ekg to package

Revision 1.14  2001/11/17 23:05:35  speedy
- updated to 20011117

Revision 1.13  2001/11/13 21:17:28  speedy
- updated to 20011112
- fixed "broken" symlinks in libgg.

Revision 1.12  2001/11/12 15:28:11  djrzulf
- corected some errors on symlinks in libgg.

Revision 1.11  2001/11/12 01:37:28  speedy
- introduced: libgg-static subpackage
- updated to 20011111

Revision 1.10  2001/11/10 22:46:02  speedy
- introduced: libgg and libgg-devel subpackage

Revision 1.9  2001/11/10 18:09:17  klakier
- ver 20011109

Revision 1.8  2001/11/05 15:26:26  djrzulf
- updated to 20011105

Revision 1.7  2001/10/28 09:54:35  undefine
- updated to 20011027
- debug ready

Revision 1.6  2001/10/21 13:38:10  undefine
- updated to 2001101402

Revision 1.5  2001/10/04 12:55:54  areq
- snapshot 2001100201

Revision 1.4  2001/10/03 19:37:27  kloczek
- cosmetics.

Revision 1.3  2001/10/02 11:24:25  areq
- snapshot 2001100103, STBR

Revision 1.2  2001/10/01 00:00:51  agaran
new snap

Revision 1.1  2001/09/29 12:04:20  areq
- init PLD spec
- snapshot 2001092902
