%define        	snapshot	20011109
Summary:	A client compatible with Gadu-Gadu 	
Summary(pl):	Eksperymentalny Klient Gadu-Gadu 	
Name:		ekg		
Version:	0.9.0.%{snapshot}
Release:	2
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
 
%prep
%setup -q -n %{name}-%{snapshot} 

%build
./configure \
	%{?!debug:--without-debug}
make
make shared

%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT%{_bindir}
install -d $RPM_BUILD_ROOT%{_includedir}
install -d $RPM_BUILD_ROOT%{_libdir}

install ekg 	$RPM_BUILD_ROOT%{_bindir}
install libgg.so.* $RPM_BUILD_ROOT%{_libdir}
ln -s %{_libdir}/libgg.so.* $RPM_BUILD_ROOT%{_libdir}/libgg.so
install libgg.h $RPM_BUILD_ROOT%{_includedir}

gzip -9nf ChangeLog README docs/* 

%post -p /sbin/ldconfig 
%postun -p /sbin/ldconfig 

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(644,root,root,755)
%doc docs/*.theme.gz
%doc ChangeLog.gz README.gz
%doc docs/7thguard.txt.gz docs/themes.txt.gz 
%doc docs/ekl.pl.gz docs/ekg.man.gz 
%attr(755,root,root) %{_bindir}/* 

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

%define date	%(echo `LC_ALL="C" date +"%a %b %d %Y"`)
