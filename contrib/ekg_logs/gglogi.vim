" syntax do vim'a do przegl±dania logów moj± przegl±dark± logów :-)
" Autor: Robert Goliasz <rgoliasz@poczta.onet.pl>

syntax on

syn match Error "^\!\!\!.*$"
syn match Notice "^-\!-.*$"

syn match RecTime "([0-9][0-9]:\?[0-9][0-9]\(:[0-9][0-9]\)\?)"
syn match Time "[0-9][0-9]:\?[0-9][0-9]\(:[0-9][0-9]\)\?"
syn match From "<.*< .*$"
syn match To   ">.*> .*$"

hi Error   ctermfg=white ctermbg=red
hi Notice  ctermfg=white
hi Time    ctermfg=darkcyan
hi RecTime ctermfg=darkblue
hi From    ctermfg=green
hi To      ctermfg=blue

noremap q :q!
map <Up> 
map <Down> 
set nomodifiable
set readonly
