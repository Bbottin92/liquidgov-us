### `/`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:60`
  - **group** (top of page)
    - link /signup 'Create an account Log in Home'
    - link /login 'Log in Home'
  - **group** (top of page)  <via genesis_body>
    - svg
    - svg
    - svg
    - button id=- '(function(){var btn=document.querySelector('.listen-pulse');'
    - audio /assets/chapter1.mp3
    - script (inline)
  - **group** h1: This is YOUR government! Here you represent yourself, and ac  <via genesis_body> (heading only)
  - **group** h2: Two clocks Site control ends when a working election exists   <via genesis_body> (heading only)
  - **group** h2: This website is the door Feed, friends, mail, and groups liv  <via genesis_body>
    - script tip.js
    - script walker.js
    - script home_drop.js
  - **group** (top of page)  <via map_page_body>
    - button id=lgm-main 'MAINstream Turn on alerts Click a box to grow its connection'
    - link /settings#map-privacy 'Map privacy a search bar at the top where you can search for'
    - form action=/search method=get
    - input name=q type=search
    - svg id=lgm-svg
    - button id=lgm-edit 'Edit Expand # Open page <button type="button" id='
    - button id=lgm-expand 'Expand # Open page &times; <iframe id="lgm-frame"'
    - link - 'Open page &times; <'
    - button id=lgm-close '&times; <span class="lgm-sc'
    - iframe lgm-frame
    - button id=lgm-forget 'Forget my visits peop'
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - link /home 'Your dashboard Colors come from what you're in and have done'
    - script lgmap.js
    - script walker.js
    - script lgmap_walk.js
  - **group** h2: People you may know &middot; sm </d  <via people_suggestions_html>
    - link /u/ '<inp'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend /friends More people' [submit]
    - link /friends 'More people'

### `/about`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:88`
  - sends you on to: "/control"; user.empty() ? login_url("/me") : "/me"

### `/admin`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:13` - operators only

### `/admin/accounts`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:18` - operators only
  - **group** (top of page)  <via admin_accounts_page>
    - form action=/admin/accounts method=get
    - input name=q type=text
    - button id=- 'Filter No accounts match. Username Created Locke' [submit]
    - table
    - link /u/ 'YesNo (you) <in'
    - form action=/admin/accounts/unlocklock method=post
    - input name=username type=hidden
    - button id=- 'UnlockLock <button class="g' [submit]
    - form action=/admin/accounts/logout-all method=post
    - input name=username type=hidden
    - button id=- 'Sign out all devices Delete&hellip;' [submit]
  - **group** summary: Delete&hellip; <input name="  <via admin_accounts_page>
    - form action=/admin/accounts/delete method=post
    - input name=username type=hidden
    - input name=confirm type=text
    - button id=- 'Delete account' [submit]

### `/admin/accounts/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:245` - operators only
  - sends you on to: "/admin/accounts"
  - builds more via (not followed): parse_form

### `/admin/accounts/lock`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:189` - operators only
  - sends you on to: "/admin/accounts"
  - builds more via (not followed): parse_form

### `/admin/accounts/logout-all`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:233` - operators only
  - sends you on to: "/admin/accounts"
  - builds more via (not followed): parse_form

### `/admin/accounts/programs`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:199` - operators only
  - sends you on to: "/admin/accounts"
  - builds more via (not followed): parse_form

### `/admin/accounts/unlock`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:223` - operators only
  - sends you on to: "/admin/accounts"
  - builds more via (not followed): parse_form

### `/admin/comments`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:53` - operators only
  - **group** (top of page)  <via admin_comments_page>
    - table
    - link - '<form method="POST" action="/admin/comments/delete" onsubmit'
    - link /u/ '<input type="hidden" name='
    - form action=/admin/comments/delete method=post
    - input name=on type=hidden
    - input name=id type=hidden
    - button id=- 'Delete' [submit]
  - **group** (top of page)  <via admin_search_form>
    - form action=(same page) method=get
    - input name=q type=text
    - button id=- 'Search' [submit]

### `/admin/comments/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:62` - operators only
  - sends you on to: "/admin/comments?done=archive-failed"; "/admin/comments?done=deleted"; "/admin/comments?done=none"
  - builds more via (not followed): parse_form

### `/admin/communities`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:28` - operators only
  - **group** (top of page)  <via admin_communities_page>
    - table
    - link /c/ '/ <summary class="muted" style="cursor:pointer'
    - link /u/ 'Delete&hellip; <form method="P'
  - **group** summary: Delete&hellip; <input name="c  <via admin_communities_page>
    - form action=/admin/communities/delete method=post
    - input name=slug type=hidden
    - input name=confirm type=text
    - button id=- 'Delete group Posts already made here keep their room name bu' [submit]

### `/admin/communities/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:271` - operators only
  - sends you on to: "/admin/communities"
  - builds more via (not followed): parse_form

### `/admin/departments`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:87` - operators only
  - **group** (top of page)  <via admin_departments_page>
    - table
    - link /city/ '<input ty'
    - form action=/admin/departments/delete method=post
    - input name=city type=hidden
    - input name=tag type=hidden
    - button id=- 'Delete' [submit]
  - **group** (top of page)  <via admin_search_form>
    - form action=(same page) method=get
    - input name=q type=text
    - button id=- 'Search' [submit]

### `/admin/departments/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:96` - operators only
  - sends you on to: "/admin/departments?done=archive-failed"; "/admin/departments?done=deleted"; "/admin/departments?done=none"
  - builds more via (not followed): parse_form

### `/admin/log`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:184` - operators only
  - **group** (top of page)  <via admin_log_page>
    - table

### `/admin/posts`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:23` - operators only
  - **group** (top of page)  <via admin_posts_page>
    - table
    - link /u/ '<input type="hidden" name="id" v'
    - form action=/admin/posts/delete method=post
    - input name=id type=hidden
    - button id=- 'Delete' [submit]
  - **group** (top of page)  <via admin_search_form>
    - form action=(same page) method=get
    - input name=q type=text
    - button id=- 'Search' [submit]

### `/admin/posts/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:260` - operators only
  - sends you on to: "/admin/posts"
  - builds more via (not followed): parse_form

### `/admin/preview`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:153` - operators only
  - **group** h2: Link preview What Facebook, X, Slack and the like show when   <via admin_preview_page> (heading only)
  - **group** h3: How it looks now <img src="" alt="" style="display:block;w  <via admin_preview_page>
    - img
  - **group** h3: Words Title (up to 120 characters; blank = each page's own t  <via admin_preview_page>
    - form action=/admin/preview method=post
    - input name=title type=text
    - textarea name=description type=textarea
    - button id=- 'Save the words Picture 1200 \xC3\x97 630 pixels works best. '
  - **group** h3: Picture 1200 \xC3\x97 630 pixels works best. JPEG or PNG, up  <via admin_preview_page>
    - form action=/admin/preview/image method=post
    - input name=image type=file
    - button id=- 'Upload the picture Remove the pictu'
    - form action=/admin/preview/image-remove method=post
    - button id=- 'Remove the picture (use the built-in one) Reset e'
    - form action=/admin/preview/reset method=post
    - button id=- 'Reset everything to the built-in preview'
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:158` - operators only
  - sends you on to: std::string("/admin/preview?done=") + (site_preview_save_text(f["title
  - builds more via (not followed): parse_form

### `/admin/preview/image`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:164` - operators only
  - sends you on to: "/admin/preview?done=" + (why.empty() ? std::string("image") : why); "/admin/preview?done=noimage"

### `/admin/preview/image-remove`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:172` - operators only
  - sends you on to: "/admin/preview?done=imagegone"

### `/admin/preview/reset`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:178` - operators only
  - sends you on to: "/admin/preview?done=reset"

### `/admin/proposals`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:33` - operators only
  - **group** (top of page)  <via admin_proposals_page>
    - table
    - link /proposals?id= 'Officials: Edit</s'
    - link /u/ 'Officials: Edit <form method="POST"'
  - **group** summary: Edit Title <input name="title" required maxlength="20  <via admin_proposals_page>
    - form action=/admin/proposals/edit method=post
    - input name=id type=hidden
    - input name=title type=text
    - textarea name=body type=textarea
    - button id=- 'Save <form method="POST" action="/admin/proposals/delete" on' [submit]
    - form action=/admin/proposals/delete method=post
    - input name=id type=hidden
    - button id=- 'Delete' [submit]

### `/admin/proposals/delete`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:41` - operators only
  - sends you on to: "/admin/proposals?done=archive-failed"; "/admin/proposals?done=deleted"; "/admin/proposals?done=none"
  - builds more via (not followed): parse_form

### `/admin/proposals/edit`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:110` - operators only
  - sends you on to: "/admin/proposals?done=edited"; "/admin/proposals?done=none"
  - builds more via (not followed): parse_form

### `/admin/styles`
- **GET** (read the page) - source `site/features/admin/admin_routes.cpp:122` - operators only
  - **group** (top of page)  <via admin_styles_page>
    - iframe /styles//preview
  - **group** h3: # by &middot; &middot; use uses report reports : <  <via admin_styles_page>
    - link /u/ '&middot; &middot; use uses report reports : (no reason given'
    - link /u/ ': (no reason given) CSS </deta'
  - **group** summary: CSS <form class="inline" method="POST" action="/admin/styles  <via admin_styles_page>
    - form action=/admin/styles/takedown method=post
    - input name=id type=hidden
    - button id=- 'Take down <button class="ghost" type="s' [submit]
    - form action=/admin/styles/dismiss method=post
    - input name=id type=hidden
    - button id=- 'Dismiss reports' [submit]

### `/admin/styles/dismiss`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:132` - operators only
  - sends you on to: "/admin/styles?done=archive-failed"; "/admin/styles?done=deleted"; "/admin/styles?done=dismissed"; "/admin/styles?done=none"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:142` - operators only
  - sends you on to: "/admin/styles?done=dismissed"

### `/admin/styles/takedown`
- **POST** (a form posts here) - source `site/features/admin/admin_routes.cpp:132` - operators only
  - sends you on to: "/admin/styles?done=archive-failed"; "/admin/styles?done=deleted"; "/admin/styles?done=dismissed"; "/admin/styles?done=none"
  - builds more via (not followed): parse_form

### `/app/civic`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:131`
  - sends you on to: "/proposals"

### `/app/mail`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:129`
  - sends you on to: "/feed"; "/mail"

### `/app/social`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:130`
  - sends you on to: "/feed"; "/proposals"

### `/apple-touch-icon-precomposed.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:64`

### `/apple-touch-icon.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:63`

### `/apps`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:123`

### `/assets/apple-touch-icon.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:65`

### `/assets/canvas.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:45`

### `/assets/chapter*`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:21`

### `/assets/draft_publish.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:41`

### `/assets/here.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:44`

### `/assets/here_media.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:47`

### `/assets/hints.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:52`

### `/assets/home_drop.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:50`

### `/assets/icon-192.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:67`

### `/assets/icon-512.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:69`

### `/assets/intro.mp4`
- **GET** (read the page) - source `site/ui/static_files.cpp:84`

### `/assets/jukebox.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:43`

### `/assets/juketube.css`
- **GET** (read the page) - source `site/ui/static_files.cpp:58`

### `/assets/juketube.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:53`

### `/assets/lgmap.css`
- **GET** (read the page) - source `site/ui/static_files.cpp:59`

### `/assets/lgmap.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:54`

### `/assets/lgmap_walk.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:48`

### `/assets/mail_widget.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:40`

### `/assets/og-image-v2.jpg`
- **GET** (read the page) - source `site/ui/static_files.cpp:82`

### `/assets/og-image-v3.jpg`
- **GET** (read the page) - source `site/ui/static_files.cpp:81`

### `/assets/og-image-v4.jpg`
- **GET** (read the page) - source `site/ui/static_files.cpp:80`

### `/assets/og-image.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:83`

### `/assets/people_search.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:42`

### `/assets/push.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:55`

### `/assets/setup.css`
- **GET** (read the page) - source `site/ui/static_files.cpp:57`

### `/assets/setup.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:39`

### `/assets/site.css`
- **GET** (read the page) - source `site/ui/static_files.cpp:56`

### `/assets/tip.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:49`

### `/assets/video_links.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:51`

### `/assets/walker.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:46`

### `/c/*`
- **GET** (read the page) - source `http_server.cpp:113`
- **GET** (read the page) - source `site/features/communities/communities_routes.cpp:116` - members only
  - sends you on to: "/c/"+slug
  - **group** (top of page)  <via canvas_tool_page>
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
  - **group** h1: what am I about to draw withwhat can I do to the canvas as a  <via canvas_tool_page>
    - button id=- '<button type="button" class="ghost'
    - button id=- '<button type="button" class="ghost to'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgrou'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="line"'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgroup-sep" aria-hi'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="bucke'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<div class="ctgroup cprops" role='
    - button id=- '<div class="row" style="flex-wrap:wrap;gap:.4rem 1rem;align-'
    - input name=- type=color
    - input name=- type=number
    - input name=- type=number
    - button id=cglow '<button type="button" class='
    - button id=csymmetry '<div class="row" id="c'
    - button id=cfill '<select id="cfont" style="widt'
    - select name=- type=select
    - button id=cbold 'B I <button ty'
    - button id=citalic 'I U</but'
    - button id=cunderline 'U <input type="color" i'
    - button id=ctextbg '<'
    - input name=- type=color
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- 'what the'
    - input name=- type=range
    - button id=cundo '&#8617; &#8618; Clear layer'
    - ... 20 more
  - **group** h2: Earlier portraits Pick one to load it into the canvas above,  <via portrait_history_owner_html>
    - button id=- '/p'
    - img
    - form action=/me/portrait/old/delete method=post
    - input name=n type=hidden
    - button id=- 'Delete <input type="checkbox" name="on" value="1"' [submit]
    - form action=/me/portrait/old/share method=post
    - input name=on type=checkbox
    - button id=- 'Save this choice' [submit]
- **GET** (read the page) - source `site/features/communities/communities_routes.cpp:123` - members only
  - **group** h1: &middot; General · / · started by · No description yet. <for
    - link /u/ '· No description yet. Join this group <form method='
    - link - 'No description yet. Join this group <form method="POST" acti'
    - form action=/c//join method=post
    - button id=- 'Join this group Leave <form method="POST" action="/c//delete'
    - form action=/c//leave method=post
    - button id=- 'Leave Delete group</'
    - form action=/c//delete method=post
    - button id=- 'Delete group Add a cityChange city <form method="POST" actio'
  - **group** summary: Add a cityChange city <input name="city" maxlength="80" list
    - form action=/c//city method=post
    - input name=city type=text
    - button id=- 'Save Groups that name the same city are listed together so p'
    - form action=/c/ method=post
    - textarea name=text type=textarea
  - **group** summary: Members · sm starter </d
    - link /u/ 'starter <input'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend Invite by account name <inpu' [submit]
    - form action=/c//invite method=post
    - input name=who type=text
    - button id=- 'Invite'
  - **group** (top of page)  <via composer_attach_bar>
    - link - '<svg width="18" height="18" viewBox="0 0 24 24" fill="none" '
    - svg
    - svg
    - input name=media type=file
    - button id=- 'Post' [submit]
- **POST** (a form posts here) - source `site/features/communities/communities_routes.cpp:216` - members only
  - sends you on to: "/c/"+slug; "/c/"+slug+"?err="+c.err; "/c/"+slug+"?err=city"; "/c/"+slug+"?err=invite"; "/communities"
  - builds more via (not followed): parse_form

### `/canvas-frame/*`
- **GET** (read the page) - source `site/features/canvas/canvas_routes.cpp:47`

### `/city`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:274` - members only
  - sends you on to: "/city/"+url_encode(key); "/communities"
  - **group** (top of page)
    - link /proposals?city= 'See itthem . They show here again once a group names the cit'
  - **group** h1: No groups name this city yet Anyone can start one. Give it a
    - link /communities 'Groups People here started these groups. A city on LiquidGov'
  - **group** h1: People here started these groups. A city on LiquidGov is a l
    - link /city//map 'Map view &middot; MAINstream group groups &middot; person pe'
    - link /map 'MAINstream group groups &middot; person people in them <a hr'
    - link /c/ '/ &middot; &middot; &middot; member members &middot; you're '
  - **group** h2: Departments and councils Named by people in this city's grou
    - form action=/city//tag method=post
    - input name=tag type=text
    - button id=- 'Add Join a group in this city to add one. None named yet. /c'
  - **group** h2: Proposals Unofficial straw poll. Discussion only. Nothing is (heading only)
  - **group** h3: Not tagged Open a proposal for this city People here People  (heading only)
  - **group** h3: Not tagged Open a proposal for this city People here People 
    - link /proposals?city= 'Open a proposal for this city People here People in these gr'
  - **group** h2: People here People in these groups who chose to be listed. N
    - form action=/city//list method=post
    - input name=on type=hidden
    - button id=- 'Stop listing meList me here Join a group in this city to be '
    - link /u/ 'you Friends <input type="hidden" name="who" value'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend Start a group in Anyone can start one' [submit]
    - link /communities?city= 'Start a group in Anyone can start one. There's no minimum si'

### `/city/*`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:251` - members only
  - sends you on to: "/city/"+url_encode(key)+"/map"; "/map"
  - **group** h1: Nothing names this city yet MAINstream MAINstream &rsaquo; <
    - link /map 'MAINstream MAINstream &rsaquo; Point at a box to light up it'
    - link /map 'MAINstream &rsaquo; Point at a box to light up its lines. Cl'
  - **group** h1: Point at a box to light up its lines. Click it to open it. L
    - link /city/ 'List view in this city member of touches <sp'
- **GET** (read the page) - source `site/features/map/map_routes.cpp:275` - members only
  - sends you on to: "/city/"+url_encode(key); "/communities"
  - **group** (top of page)
    - link /proposals?city= 'See itthem . They show here again once a group names the cit'
  - **group** h1: No groups name this city yet Anyone can start one. Give it a
    - link /communities 'Groups People here started these groups. A city on LiquidGov'
  - **group** h1: People here started these groups. A city on LiquidGov is a l
    - link /city//map 'Map view &middot; MAINstream group groups &middot; person pe'
    - link /map 'MAINstream group groups &middot; person people in them <a hr'
    - link /c/ '/ &middot; &middot; &middot; member members &middot; you're '
  - **group** h2: Departments and councils Named by people in this city's grou
    - form action=/city//tag method=post
    - input name=tag type=text
    - button id=- 'Add Join a group in this city to add one. None named yet. /c'
  - **group** h2: Proposals Unofficial straw poll. Discussion only. Nothing is (heading only)
  - **group** h3: Not tagged Open a proposal for this city People here People  (heading only)
  - **group** h3: Not tagged Open a proposal for this city People here People 
    - link /proposals?city= 'Open a proposal for this city People here People in these gr'
  - **group** h2: People here People in these groups who chose to be listed. N
    - form action=/city//list method=post
    - input name=on type=hidden
    - button id=- 'Stop listing meList me here Join a group in this city to be '
    - link /u/ 'you Friends <input type="hidden" name="who" value'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend Start a group in Anyone can start one' [submit]
    - link /communities?city= 'Start a group in Anyone can start one. There's no minimum si'
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:405` - members only
  - sends you on to: "/city/"+url_encode(key); "/city/"+url_encode(key)+"?err=list"; "/communities"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:417` - members only
  - sends you on to: "/communities"; dest; dest+"?err=ctag"; dest+"?err=ctagname"
  - builds more via (not followed): parse_form

### `/communities`
- **GET** (read the page) - source `site/features/communities/communities_routes.cpp:17` - members only
  - **group** h1: Groups Platform groups. Not AVND localities, and they do not (heading only)
  - **group** h2: Yours / <d
    - link /c/ '/ Cities Groups that name the same city are lis'
  - **group** h2: Cities Groups that name the same city are listed together, s
    - link /map 'See them on the MAINstream map . &middot; Find a group'
    - link /city/ '&middot; Find a group <input id="commfilter" type="search" p'
  - **group** h2: Find a group <input id="commfilter" type="search" placeholde
    - input name=- type=search
  - **group** summary: &middot; General <stro
    - link /c/ '/ Start a group herecity <div'
    - link - 'Start a group herecity Start a group <form method="POS'
  - **group** summary: Start a group Name <input name="name" required maxlength="80
    - form action=/communities method=post
    - input name=name type=text
    - input name=slug type=text
    - select name=category type=select
    - input name=subcategory type=text
    - input name=city type=text
    - textarea name=about type=textarea
    - button id=- 'Create (function(){var SUB=;var sel=document.getElementById('
    - script (inline)
- **POST** (a form posts here) - source `site/features/communities/communities_routes.cpp:196` - members only
  - sends you on to: "/c/"+slug; "/communities?err=address"; "/communities?err=subcategory"
  - builds more via (not followed): parse_form

### `/community/ws`
- **GET** (read the page) - source `http_server.cpp:82`

### `/control`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:85`
  - **group** h1: Who holds control Genesis operators run the machines and say  <via control_body>
    - table
    - link /login?next=/export 'export or delete from Settings. Voting Nothing is awaiting y'
  - **group** h3: Nothing is awaiting your vote. No election has been held. Th  <via control_body>
    - link /login?next=/proposals 'Proposals are an unofficial straw poll any member can open o'
  - **group** h1: What we keep Not a government. A platform account is not a c  <via privacy_body> (heading only)
  - **group** h2: Sign-in Username and password. A phone is bound at signup so  <via privacy_body> (heading only)
  - **group** h2: What you put here Posts, mail, and drawings stay until you d  <via privacy_body>
    - link /login?next=/export 'export or delete from Settings . The operator of this copy i'
    - link /login?next=/settings 'Settings . The operator of this copy is named on who holds c'
    - link /control 'who holds control .'

### `/drop/off`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:101`
  - sends you on to: back, extra

### `/drop/on`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:101`
  - sends you on to: back, extra

### `/export`
- **GET** (read the page) - source `site/features/settings/settings_routes.cpp:210` - members only

### `/favicon.ico`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:11`
  - sends you on to: "/favicon.svg"

### `/favicon.svg`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:15`

### `/feed`
- **GET** (read the page) - source `site/features/feed/feed_routes.cpp:31` - members only
  - **group** (top of page)
    - link /feed?filter= 'Feed &middot; members allAllfriendsFriendscommitteesGroups <'
  - **group** h1: Feed &middot; members allAllfriendsFriendscommitteesGroups N
    - link /friends 'add a friend so theirs show up. Feed'
  - **group** (top of page)  <via feed_composer_card>
    - button id=- 'vis-groupsGroups Which groups <p class="muted" style="margin'
  - **group** summary: Which groups Pick one or more rooms you belong to. group <fo  <via feed_composer_card>
    - form action=/feed method=post
  - **group** summary: Exceptions Hide-list &mdash; checked friends won't see this,  <via feed_composer_card> (heading only)
  - **group** summary: Which friends Show-list &mdash; leave everyone unchecked to   <via feed_composer_card>
    - input name=anybody type=hidden
    - input name=friends type=hidden
    - input name=groups type=hidden
    - textarea name=text type=textarea
    - script (inline)
    - button id=feedattachbtn 'Post <p class="muted" id="feedfilena'
    - button id=- 'Post <p class="muted" id="feedfilename" style="margin:.3rem ' [submit]
    - input name=media type=file
    - link /feed/canvas 'open the canvas to draw, crop, layer, or paste one together '
    - script (inline)
  - **group** (top of page)  <via composer_checkbox_list>
    - input name=- type=checkbox
- **POST** (a form posts here) - source `site/features/feed/feed_routes.cpp:163` - members only
  - sends you on to: "/feed"; "/feed?err=nogroup"; std::string("/feed?err=")+c.err
  - builds more via (not followed): parse_form

### `/feed/canvas`
- **GET** (read the page) - source `site/features/canvas/canvas_routes.cpp:21` - members only
  - **group** (top of page)  <via canvas_tool_page>
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
  - **group** h1: what am I about to draw withwhat can I do to the canvas as a  <via canvas_tool_page>
    - button id=- '<button type="button" class="ghost'
    - button id=- '<button type="button" class="ghost to'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgrou'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="line"'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgroup-sep" aria-hi'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="bucke'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<div class="ctgroup cprops" role='
    - button id=- '<div class="row" style="flex-wrap:wrap;gap:.4rem 1rem;align-'
    - input name=- type=color
    - input name=- type=number
    - input name=- type=number
    - button id=cglow '<button type="button" class='
    - button id=csymmetry '<div class="row" id="c'
    - button id=cfill '<select id="cfont" style="widt'
    - select name=- type=select
    - button id=cbold 'B I <button ty'
    - button id=citalic 'I U</but'
    - button id=cunderline 'U <input type="color" i'
    - button id=ctextbg '<'
    - input name=- type=color
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- 'what the'
    - input name=- type=range
    - button id=cundo '&#8617; &#8618; Clear layer'
    - ... 20 more
  - **group** h2: Earlier portraits Pick one to load it into the canvas above,  <via portrait_history_owner_html>
    - button id=- '/p'
    - img
    - form action=/me/portrait/old/delete method=post
    - input name=n type=hidden
    - button id=- 'Delete <input type="checkbox" name="on" value="1"' [submit]
    - form action=/me/portrait/old/share method=post
    - input name=on type=checkbox
    - button id=- 'Save this choice' [submit]

### `/forgot`
- **GET** (read the page) - source `site/features/auth/auth_routes.cpp:36`
  - **group** h1: Forgot your password? Used Google? Sign in with Google inste
    - link /login 'Sign in with Google instead &mdash; no reset needed. Email <'
    - form action=/forgot method=post
    - input name=email type=email
    - button id=- 'Send reset link Back to log in'
    - link /login 'Back to log in'
- **POST** (a form posts here) - source `site/features/auth/auth_routes.cpp:153`
  - **group** h1: Check your email If that address is on an account, a link is
    - link /login 'Back to log in'
  - builds more via (not followed): parse_form

### `/friends`
- **GET** (read the page) - source `site/features/people/people_routes.cpp:13` - members only
  - **group** h1: People /friends Find someone Type a name <input name="q" id= (heading only)
  - **group** h2: Find someone Type a name <input name="q" id="peopleq" autoco
    - form action=/search method=get
    - input name=q type=text
    - button id=- 'Search Friends always appear. Everyone else follows their fi'
  - **group** h2: Add by exact name Send request</butto
    - form action=/friends method=post
    - input name=who type=text
    - button id=- 'Send request Friends friend friends &middot; inner No friend'
  - **group** h2: Friends friend friends &middot; inner No friends yet. Search
    - link /u/ 'inner circle Mail <form class="inline" m'
    - link /mail?with= 'Mail From inne'
    - form action=/friends/inner-remove method=post
    - input name=who type=hidden
    - button id=- 'From inner <button class="ghost" type="submit' [submit]
    - form action=/friends/inner method=post
    - input name=who type=hidden
    - button id=- 'Inner circle <form class="inline" method="POST" action="/fri' [submit]
    - form action=/friends/unfriend method=post
    - input name=who type=hidden
    - button id=- 'Unfriend <button class="danger" type="submit"' [submit]
    - form action=/friends/block method=post
    - input name=who type=hidden
    - button id=- 'Block Inner circle is up to twelve people. Everyone else you' [submit]
  - **group** h2: Incoming None. sm <form class="inline" met
    - link /u/ '<input type="hidden" name="who" valu'
    - form action=/friends/accept method=post
    - input name=who type=hidden
    - button id=- 'Accept' [submit]
    - form action=/friends/decline method=post
    - input name=who type=hidden
    - button id=- 'Decline Outgoing If someone does not accept, you will not ge' [submit]
  - **group** h2: Outgoing If someone does not accept, you will not get a noti (heading only)
  - **group** h2: People you may know &middot; sm </d  <via people_suggestions_html>
    - link /u/ '<inp'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend /friends More people' [submit]
    - link /friends 'More people'
  - builds more via (not followed): who_html
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:101` - members only
  - sends you on to: dest
  - builds more via (not followed): parse_form

### `/friends/accept`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:113` - members only
  - sends you on to: "/friends"
  - builds more via (not followed): parse_form

### `/friends/block`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:135` - members only
  - sends you on to: "/friends"
  - builds more via (not followed): parse_form

### `/friends/decline`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:122` - members only
  - sends you on to: "/friends"
  - builds more via (not followed): parse_form

### `/friends/inner`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:141` - members only
  - sends you on to: "/friends"; "/friends?err=inner"
  - builds more via (not followed): parse_form

### `/friends/inner-remove`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:147` - members only
  - sends you on to: "/friends"
  - builds more via (not followed): parse_form

### `/friends/unfriend`
- **POST** (a form posts here) - source `site/features/people/people_routes.cpp:130` - members only
  - sends you on to: "/friends"
  - builds more via (not followed): parse_form

### `/here/choice`
- **POST** (a form posts here) - source `site/features/here/here_routes.cpp:19` - members only
  - sends you on to: back
  - builds more via (not followed): parse_form

### `/here/ticket`
- **GET** (read the page) - source `site/features/here/here_routes.cpp:30` - members only

### `/home`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:53` - members only
  - **group** h2: What this is Genesis, not a government. Three rules, and the
    - link /laws 'Read the disclosure'

### `/icon-192.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:66`

### `/icon-512.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:68`

### `/icon-civic-192.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:77`

### `/icon-civic-512.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:78`

### `/icon-civic.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:76`

### `/icon-mail-192.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:71`

### `/icon-mail-512.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:72`

### `/icon-mail.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:70`

### `/icon-social-192.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:74`

### `/icon-social-512.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:75`

### `/icon-social.png`
- **GET** (read the page) - source `site/ui/static_files.cpp:73`

### `/install`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:123`

### `/internal/upload-ok`
- **GET** (read the page) - source `site/features/system/system_routes.cpp:13`

### `/jukebox/browse`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:136` - members only

### `/jukebox/channel`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:192` - members only
  - sends you on to: "/"; "/me"; "/me?err=full"; "/me?err=url"; "/me?ok=jplay"; "/settings"; "/u/"+who; "/u/"+who+"?err=denied"
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select
  - builds more via (not followed): parse_form

### `/jukebox/history`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:145` - members only

### `/jukebox/meta`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:73` - members only
  - sends you on to: "/"; "/me"; "/me?err=full"; "/me?err=url"; "/me?ok=jplay"; "/settings"; "/u/"+who; "/u/"+who+"?err=denied"
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select
  - builds more via (not followed): parse_form

### `/jukebox/oembed`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:215` - members only
  - sends you on to: "/"; "/me"; "/me?err=full"; "/me?err=url"; "/me?ok=jplay"; "/settings"; "/u/"+who; "/u/"+who+"?err=denied"
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select
  - builds more via (not followed): parse_form

### `/jukebox/people`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:111` - members only
  - sends you on to: "/"; "/me"; "/me?err=full"; "/me?err=url"; "/me?ok=jplay"; "/settings"; "/u/"+who; "/u/"+who+"?err=denied"
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select
  - builds more via (not followed): parse_form

### `/jukebox/search`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:18` - members only
  - sends you on to: "/"; "/me"; "/me?err=full"; "/me?err=url"; "/me?ok=jplay"; "/settings"; "/u/"+who; "/u/"+who+"?err=denied"
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select
  - builds more via (not followed): parse_form

### `/laws`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:76`
  - **group** (top of page)  <via laws_body>
    - link /laws/ '~ min min A Voluntary Natural Democracy The'
  - **group** h1: A Voluntary Natural Democracy The three universal laws are t  <via laws_body> (heading only)
  - **group** h2: Chapters Who holds control of this site · Genesis disclosure  <via laws_body>
    - link /control 'Who holds control of this site · Genesis disclosure'
    - link / 'Genesis disclosure'
  - **group** (top of page)  <via laws_chapter_body>
    - link /laws '&larr; Back to overview Next: &rarr; Chapter of <p class="mu'
    - link /laws/ 'Next: &rarr; Chapter of You can stop here. Most people do, a'
  - **group** h1: You can stop here. Most people do, and that's fine — the res  <via laws_chapter_body> (heading only)
  - **group** (top of page)  <via chapter_narration_html>
    - button id=narrbtn '&#9654; Listen to this chapter'
    - audio /assets/chapter.mp3
    - script (inline)

### `/laws/*`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:77`
  - **group** (top of page)  <via laws_chapter_body>
    - link /laws '&larr; Back to overview Next: &rarr; Chapter of <p class="mu'
    - link /laws/ 'Next: &rarr; Chapter of You can stop here. Most people do, a'
  - **group** h1: You can stop here. Most people do, and that's fine — the res  <via laws_chapter_body> (heading only)
  - **group** (top of page)  <via chapter_narration_html>
    - button id=narrbtn '&#9654; Listen to this chapter'
    - audio /assets/chapter.mp3
    - script (inline)

### `/listen`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:242` - members only
  - **group** (top of page)  <via jukebox_panel_body>
    - button id=jbtab-community 'Stations Search <button type="button" class="ghos'
    - button id=jbtab-search 'Search Float above <button type="button" class='
    - button id=jbfloat 'Float above \xE2\x9C\x95 <div id="jbplayer" style="margin-to'
    - button id=jbclose '\xE2\x9C\x95 <div class="row" s'
    - button id=jbplaypause 'Play Mute <d'
    - button id=jbmute 'Mute on the phone<p class="muted" style="ma'
    - input name=- type=range
    - select name=- type=select
    - input name=- type=text
    - button id=jbfollowbtn 'Follow Connecting&helli'
    - input name=- type=text
    - select name=- type=select
    - input name=- type=text
    - select name=- type=select

### `/log`
- **POST** (a form posts here) - source `site/features/system/system_routes.cpp:27` - members only

### `/login`
- **GET** (read the page) - source `site/features/auth/auth_routes.cpp:16`
  - **group** h1: Log in Log in with your username and password. Username <inp
    - form action=/login method=post
    - input name=username type=text
    - input name=password type=password
    - input name=next type=hidden
    - button id=- 'Continue Forgot password? &middot; New here? Create an accou'
    - link /forgot 'Forgot password? &middot; New here? Create an account .'
    - link /signup 'Create an account .'
  - **group** (top of page)  <via google_signin_button_html>
    - script https://accounts.google.com/gsi/client
- **POST** (a form posts here) - source `site/features/auth/auth_routes.cpp:132`
  - sends you on to: dest, set_cookie(r.session->id); loc
  - builds more via (not followed): parse_form

### `/login/google`
- **POST** (a form posts here) - source `site/features/auth/auth_routes.cpp:193`
  - sends you on to: "/", set_cookie(session.id); "/login?err=google"; "/login?err=google-down"
  - builds more via (not followed): parse_form

### `/logout`
- **GET** (read the page) - source `site/features/auth/auth_routes.cpp:59`
  - sends you on to: "/", clear_cookie()

### `/logs`
- **GET** (read the page) - source `site/features/system/system_routes.cpp:19` - operators only

### `/mail`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:38` - members only
  - **group** (top of page)
    - link /search?q=with 'Find with MessagesMailmore Messages Direct messages are frie'
  - **group** h1: Messages Direct messages are friends-only for now. more Open (heading only)
  - **group** summary: more Open messaging waits on age assurance so a stranger can (heading only)
  - **group** (top of page)  <via mail_inbox_html>
    - link /mail?with= 'sm <div class="mails'
    - link /mail/g/ '· invite</sp'
    - link /friends 'Find people Inner circle Friends'
  - **group** (top of page)  <via mail_thread_html>
    - link /mail '\xE2\x86\x90 sm errerrnotfriends Mail is friends-only until '
    - link /u/ 'errerrnotfriends Mail is friends-only until they accept. No '
    - form action=/mail method=post
    - input name=to type=hidden
    - textarea name=text type=textarea
    - button id=- 'Send it shouldnt say that you can send programs or have // i' [submit]
    - script (inline)
  - **group** (top of page)  <via mail_attachment_html>
    - link - 'Download attachment \xF0\x9F\x93\xA6 <span class="muted"'
    - link - '\xF0\x9F\x93\xA6 \xC2\xB7 A program from . Open it only if y'
  - **group** (top of page)  <via mail_file_input_html>
    - input name=attachment type=file
- **POST** (a form posts here) - source `site/features/mail/mail_routes.cpp:185` - members only
  - sends you on to: "/mail?with="+to; "/mail?with="+to+"&err="+code; "/mail?with="+to+"&err=notfriends"
  - builds more via (not followed): parse_form

### `/mail/attachment/*`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:69` - members only

### `/mail/big`
- **POST** (a form posts here) - source `site/router.cpp:45`

### `/mail/big/*`
- **POST** (a form posts here) - source `site/router.cpp:45`

### `/mail/g/*`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:90` - members only
  - **group** (top of page)
    - link /mail '\xE2\x86\x90 Mail Platform group conversation. Not an AVND l'
  - **group** h1: Platform group conversation. Not an AVND locality. errinvite
    - form action=/accept method=post
    - button id=- 'Accept Decline <div class="msglist" id="msglist"'
    - form action=/leave method=post
    - button id=- 'Decline <strong'
    - form action=/send method=post
    - textarea name=text type=textarea
    - button id=- 'Send it shouldnt say that you can send programs or // have i' [submit]
  - **group** summary: Members invited \xE2\x80\x94 not in yet Invite</
    - form action=/invite method=post
    - input name=who type=text
    - button id=- 'Invite Group name <butto'
    - form action=/title method=post
    - input name=title type=text
    - button id=- 'Save name <button class='
    - form action=/leave method=post
    - button id=- 'Leave group (function(){var list=document.getElementById('ms'
    - script (inline)
  - **group** (top of page)  <via mail_attachment_html>
    - link - 'Download attachment \xF0\x9F\x93\xA6 <span class="muted"'
    - link - '\xF0\x9F\x93\xA6 \xC2\xB7 A program from . Open it only if y'
  - **group** (top of page)  <via mail_file_input_html>
    - input name=attachment type=file
- **POST** (a form posts here) - source `site/features/mail/mail_routes.cpp:289` - members only
  - sends you on to: "/mail"; base; base+"?err="+code; refuse ? (base+"?err=invite") : base
  - builds more via (not followed): parse_form

### `/mail/group/new`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:16` - members only
  - sends you on to: "/mail"
- **POST** (a form posts here) - source `site/features/mail/mail_routes.cpp:234` - members only
  - sends you on to: loc
  - builds more via (not followed): parse_form

### `/mail/panel`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:61` - members only
  - **group** (top of page)  <via mail_inbox_html>
    - link /mail?with= 'sm <div class="mails'
    - link /mail/g/ '· invite</sp'
    - link /friends 'Find people Inner circle Friends'
  - **group** (top of page)  <via mail_thread_html>
    - link /mail '\xE2\x86\x90 sm errerrnotfriends Mail is friends-only until '
    - link /u/ 'errerrnotfriends Mail is friends-only until they accept. No '
    - form action=/mail method=post
    - input name=to type=hidden
    - textarea name=text type=textarea
    - button id=- 'Send it shouldnt say that you can send programs or have // i' [submit]
    - script (inline)
  - **group** (top of page)  <via mail_attachment_html>
    - link - 'Download attachment \xF0\x9F\x93\xA6 <span class="muted"'
    - link - '\xF0\x9F\x93\xA6 \xC2\xB7 A program from . Open it only if y'
  - **group** (top of page)  <via mail_file_input_html>
    - input name=attachment type=file

### `/mail/too-large`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:29`
  - **group** h1: Not sent Back to mail
    - link /mail 'Back to mail'

### `/manifest-civic.webmanifest`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:72`

### `/manifest-mail.webmanifest`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:40`

### `/manifest-social.webmanifest`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:59`

### `/manifest.webmanifest`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:18`

### `/map`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:40` - members only
  - **group** (top of page)  <via map_page_body>
    - button id=lgm-main 'MAINstream Turn on alerts Click a box to grow its connection'
    - link /settings#map-privacy 'Map privacy a search bar at the top where you can search for'
    - form action=/search method=get
    - input name=q type=search
    - svg id=lgm-svg
    - button id=lgm-edit 'Edit Expand # Open page <button type="button" id='
    - button id=lgm-expand 'Expand # Open page &times; <iframe id="lgm-frame"'
    - link - 'Open page &times; <'
    - button id=lgm-close '&times; <span class="lgm-sc'
    - iframe lgm-frame
    - button id=lgm-forget 'Forget my visits peop'
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - link /home 'Your dashboard Colors come from what you're in and have done'
    - script lgmap.js
    - script walker.js
    - script lgmap_walk.js
  - **group** h2: People you may know &middot; sm </d  <via people_suggestions_html>
    - link /u/ '<inp'
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend /friends More people' [submit]
    - link /friends 'More people'

### `/map/instance-css`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:205` - operators only
  - builds more via (not followed): parse_form

### `/map/node`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:230` - members only

### `/map/search`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:223` - members only

### `/me`
- **GET** (read the page) - source `site/features/profile/profile_routes.cpp:20` - members only
  - **group** h1: Platform profile · public page · member since Self-portrait 
    - link /u/ 'public page · member since Self-portrait &mdash; draw one wi'
    - link /me/portrait 'portrait tool . Remove portrait <p class="mut'
    - form action=/me/avatar/clear method=post
    - button id=- 'Remove portrait A platform account is not a citizen identity' [submit]
  - **group** h2: About you Name (people can find you by it) <input name="dis
    - form action=/me/profile method=post
    - input name=display_name type=text
    - input name=hometown type=text
    - input name=living type=text
    - textarea name=bio type=textarea
    - input name=interests type=text
    - input name=career type=text
    - input name=school type=text
  - **group** h2: Canvas Open the canvas okcss Saved. See it at y
    - link /me/canvas 'Open the canvas okcss Saved. See it at your public profile .'
    - link /u/ 'your public profile . Your saved CSS isn't shown on your pro'
  - **group** h2: Profile CSS i dont really see anything with the style librar
    - link /styles 'Browse styles Styles your whole profile page (/u/) below the'
    - form action=/me/css method=post
    - textarea name=css type=textarea
  - **group** h2: Play settings Play my
    - form action=/me/jukebox/play-settings method=post
    - input name=autoplay type=checkbox
    - input name=relay type=text
  - **group** (top of page)  <via styles_me_card>
    - link /u/ '&rsquo;s . Share your saved CS'
    - link /styles#style- '. Share your saved CSS in the style libra'
    - form action=/styles/publish method=post
    - link /styles 'style library <input id="style-name" name="name" required ma'
    - input name=name type=text
    - button id=- 'Publish Publishing shares a copy of your CSS as it is now, w' [submit]
    - link /styles 'style library .'

### `/me/avatar`
- **POST** (a form posts here) - source `site/features/profile/profile_routes.cpp:316` - members only
  - sends you on to: "/me"; "/me?err=image"; "/me?err=portraitlarge"; "/me?err=scan"

### `/me/avatar/clear`
- **POST** (a form posts here) - source `site/features/profile/profile_routes.cpp:328` - members only
  - sends you on to: "/me"

### `/me/canvas`
- **GET** (read the page) - source `site/features/canvas/canvas_routes.cpp:13` - members only
  - **group** (top of page)  <via canvas_tool_page>
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
  - **group** h1: what am I about to draw withwhat can I do to the canvas as a  <via canvas_tool_page>
    - button id=- '<button type="button" class="ghost'
    - button id=- '<button type="button" class="ghost to'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgrou'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="line"'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgroup-sep" aria-hi'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="bucke'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<div class="ctgroup cprops" role='
    - button id=- '<div class="row" style="flex-wrap:wrap;gap:.4rem 1rem;align-'
    - input name=- type=color
    - input name=- type=number
    - input name=- type=number
    - button id=cglow '<button type="button" class='
    - button id=csymmetry '<div class="row" id="c'
    - button id=cfill '<select id="cfont" style="widt'
    - select name=- type=select
    - button id=cbold 'B I <button ty'
    - button id=citalic 'I U</but'
    - button id=cunderline 'U <input type="color" i'
    - button id=ctextbg '<'
    - input name=- type=color
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- 'what the'
    - input name=- type=range
    - button id=cundo '&#8617; &#8618; Clear layer'
    - ... 20 more
  - **group** h2: Earlier portraits Pick one to load it into the canvas above,  <via portrait_history_owner_html>
    - button id=- '/p'
    - img
    - form action=/me/portrait/old/delete method=post
    - input name=n type=hidden
    - button id=- 'Delete <input type="checkbox" name="on" value="1"' [submit]
    - form action=/me/portrait/old/share method=post
    - input name=on type=checkbox
    - button id=- 'Save this choice' [submit]

### `/me/canvas/clear`
- **POST** (a form posts here) - source `site/features/canvas/canvas_routes.cpp:68` - members only
  - sends you on to: "/me/canvas"

### `/me/canvas/frame`
- **POST** (a form posts here) - source `site/features/canvas/canvas_routes.cpp:73` - members only
  - sends you on to: "/me/canvas?err=framefull"; "/me/canvas?err=frameimage"; "/me/canvas?err=framelarge"

### `/me/canvas/mode`
- **POST** (a form posts here) - source `site/features/canvas/canvas_routes.cpp:99` - members only
  - sends you on to: "/me/canvas?ok=1"
  - builds more via (not followed): parse_form

### `/me/css`
- **POST** (a form posts here) - source `site/features/profile/profile_routes.cpp:307` - members only
  - sends you on to: "/me?err=css&reason=" + check.code; "/me?ok=css"
  - builds more via (not followed): parse_form

### `/me/jukebox/add`
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:299` - members only
  - sends you on to: "/me"; "/me?err=full"; "/me?err=url"
  - builds more via (not followed): parse_form

### `/me/jukebox/play-settings`
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:312` - members only
  - sends you on to: "/me?ok=jplay"
  - builds more via (not followed): parse_form

### `/me/jukebox/policy`
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:319` - members only
  - sends you on to: "/settings"
  - builds more via (not followed): parse_form

### `/me/jukebox/remove`
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:307` - members only
  - sends you on to: "/me"
  - builds more via (not followed): parse_form

### `/me/portrait`
- **GET** (read the page) - source `site/features/canvas/canvas_routes.cpp:17` - members only
  - **group** (top of page)  <via canvas_tool_page>
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
    - svg
  - **group** h1: what am I about to draw withwhat can I do to the canvas as a  <via canvas_tool_page>
    - button id=- '<button type="button" class="ghost'
    - button id=- '<button type="button" class="ghost to'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgrou'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="line"'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<span class="toolgroup-sep" aria-hi'
    - button id=- '<button type="button" class="ghost toolbtn" data-tool="bucke'
    - button id=- '<button type="button" class="ghost'
    - button id=- '<div class="ctgroup cprops" role='
    - button id=- '<div class="row" style="flex-wrap:wrap;gap:.4rem 1rem;align-'
    - input name=- type=color
    - input name=- type=number
    - input name=- type=number
    - button id=cglow '<button type="button" class='
    - button id=csymmetry '<div class="row" id="c'
    - button id=cfill '<select id="cfont" style="widt'
    - select name=- type=select
    - button id=cbold 'B I <button ty'
    - button id=citalic 'I U</but'
    - button id=cunderline 'U <input type="color" i'
    - button id=ctextbg '<'
    - input name=- type=color
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- '<div style="di'
    - input name=- type=range
    - button id=- 'what the'
    - input name=- type=range
    - button id=cundo '&#8617; &#8618; Clear layer'
    - ... 20 more
  - **group** h2: Earlier portraits Pick one to load it into the canvas above,  <via portrait_history_owner_html>
    - button id=- '/p'
    - img
    - form action=/me/portrait/old/delete method=post
    - input name=n type=hidden
    - button id=- 'Delete <input type="checkbox" name="on" value="1"' [submit]
    - form action=/me/portrait/old/share method=post
    - input name=on type=checkbox
    - button id=- 'Save this choice' [submit]

### `/me/portrait/old/*`
- **GET** (read the page) - source `site/features/profile/portrait_history.cpp:39` - members only

### `/me/portrait/old/delete`
- **POST** (a form posts here) - source `site/features/profile/portrait_history.cpp:65` - members only
  - sends you on to: "/me/portrait"
  - builds more via (not followed): parse_form

### `/me/portrait/old/share`
- **POST** (a form posts here) - source `site/features/profile/portrait_history.cpp:74` - members only
  - sends you on to: "/me/portrait"
  - builds more via (not followed): parse_form

### `/me/profile`
- **POST** (a form posts here) - source `site/features/profile/profile_routes.cpp:291` - members only
  - sends you on to: "/me"; "/me?err=profile"
  - builds more via (not followed): parse_form

### `/me/repost`
- **POST** (a form posts here) - source `site/features/feed/feed_routes.cpp:95` - members only
  - sends you on to: "/me"; back_to_post(header_value(req, "Referer"), pid)
  - builds more via (not followed): parse_form

### `/me/unread`
- **GET** (read the page) - source `site/features/mail/mail_routes.cpp:20` - members only

### `/notices`
- **GET** (read the page) - source `site/features/notices/notices_routes.cpp:10` - members only
  - **group** h1: Notices (heading only)
  - **group** (top of page)  <via notices_list_html>
    - link - '<input t'
    - form action=/friends/accept method=post
    - input name=who type=hidden
    - button id=- 'Accept A declined friend request does not send a notice.' [submit]

### `/notices/panel`
- **GET** (read the page) - source `site/features/notices/notices_routes.cpp:22` - members only
  - **group** (top of page)  <via notices_list_html>
    - link - '<input t'
    - form action=/friends/accept method=post
    - input name=who type=hidden
    - button id=- 'Accept A declined friend request does not send a notice.' [submit]

### `/nsfw/off`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:113`
  - sends you on to: back, extra

### `/nsfw/on`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:113`
  - sends you on to: back, extra

### `/officials`
- **GET** (read the page) - source `site/features/proposals/proposals_routes.cpp:207`
  - **group** h2: name · countcandidate No races yet. <h2
    - input name=next type=hidden
  - **group** h2: Open a race Anyone with an account can vote. Not a certified
    - form action=/officials method=post
    - input name=title type=text
    - input name=place type=text
    - textarea name=options type=textarea
    - input name=open type=checkbox
    - button id=- 'Open race Log in to vote or open a race. Officials Officials' [submit]
    - link /login?next=/officials 'Log in to vote or open a race. Officials Officials Account v'
  - **group** h1: Officials Account vote only. Not a certified election and no (heading only)
  - **group** (top of page)  <via choice_ballot_body>
    - form action=/proposals/vote method=post
    - input name=id type=hidden
    - input name=value type=hidden
    - button id=- '· &#10003; · None of these · Skip / no preference None of th' [submit]
    - form action=/proposals/option method=post
    - input name=id type=hidden
    - input name=label type=text
    - button id=- 'Add You already added a on this one.' [submit]
  - **group** (top of page)  <via reserved_answer_form>
    - form action=/proposals/vote method=post
    - input name=id type=hidden
    - input name=value type=hidden
    - button id=- '&#10003;' [submit]
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:242` - members only
  - sends you on to: "/officials"; "/officials?err=dupe"; "/officials?err=options"
  - builds more via (not followed): parse_form

### `/pain/*`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:178` - members only
  - sends you on to: "/problems#pain" + std::to_string(p->id)
  - **group** h1: Flowed by . This is the subject they chose to show. See it o
    - link /u/ '. This is the subject they chose to show. See it on the map'
    - link /map?focus=pain: 'See it on the map'
  - **group** (top of page)  <via also_flowed_html>
    - link /u/ '.'

### `/portrait-history/*`
- **GET** (read the page) - source `site/features/profile/portrait_history.cpp:46`

### `/portrait/*`
- **GET** (read the page) - source `site/features/canvas/canvas_routes.cpp:26`

### `/post-media/*`
- **GET** (read the page) - source `site/features/feed/feed_routes.cpp:66`

### `/post/*`
- **GET** (read the page) - source `site/features/feed/feed_routes.cpp:84` - members only
- **POST** (a form posts here) - source `site/features/feed/feed_routes.cpp:127` - members only
  - sends you on to: back; back_to_post(back, pid)
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/feed/feed_routes.cpp:139` - members only
  - sends you on to: back; back_to_post(back, pid)
- **POST** (a form posts here) - source `site/features/feed/feed_routes.cpp:150` - members only
  - sends you on to: back

### `/preview-image`
- **GET** (read the page) - source `site/features/preview/preview_routes.cpp:9`

### `/privacy`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:86`
  - **group** h1: What we keep Not a government. A platform account is not a c  <via privacy_body> (heading only)
  - **group** h2: Sign-in Username and password. A phone is bound at signup so  <via privacy_body> (heading only)
  - **group** h2: What you put here Posts, mail, and drawings stay until you d  <via privacy_body>
    - link /login?next=/export 'export or delete from Settings . The operator of this copy i'
    - link /login?next=/settings 'Settings . The operator of this copy is named on who holds c'
    - link /control 'who holds control .'
  - **group** h1: How this association works LiquidGov.US is a voluntary assoc  <via terms_body>
    - link /control 'who holds control . It is not an office. The three laws · Wh'
    - link /laws 'The three laws · What we keep'
    - link /privacy 'What we keep'

### `/problems`
- **GET** (read the page) - source `site/features/map/map_routes.cpp:51` - members only
  - **group** h1: Your problems and solutions Things you said you wish for or 
    - link /settings#map-privacy 'Turn it on in Settings . It reads your new posts, proposals,'
    - link /settings#map-privacy 'Change this Look at what I've written This reads posts,'
    - form action=/problems/look method=post
    - button id=- 'Look at what I've written This reads posts, proposals, propo' [submit]
    - link /post/ 'post:from your postfpost:from a friends postgpost:from a gro'
    - link /proposals?id= 'from your proposal mail: from a message you sent gmsg: from '
    - form action=/problems/ method=post
    - input name=id type=hidden
    - button id=- 'keepKeepforgetForget <p class'
    - link /proposals?city= 'Proposals about it <p class="muted" style'
    - link /proposals?id= 'Groups working on it No possible solution'
    - link /c/ 'No possible solutions on the site yet. You could start one. '
  - **group** h3: Start a proposal about this &middot; See it on your map f
    - link - 'Start a proposal about this &middot; See it on your map forg'
    - link /map?focus=pain: 'See it on your map forgetForget Noticed Keep the ones that a'
  - **group** h2: Noticed Keep the ones that are real. Ones you don't keep dis (heading only)
  - **group** h2: Kept Nothing yet. When you write something like &ldquo;I wis (heading only)
  - **group** (top of page)  <via also_flowed_html>
    - link /u/ '.'

### `/problems/flow`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:149` - members only
  - sends you on to: stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:159` - members only

### `/problems/forget`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:149` - members only
  - sends you on to: stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:158` - members only

### `/problems/keep`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:149` - members only
  - sends you on to: stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:157` - members only

### `/problems/look`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:169` - members only
  - sends you on to: "/problems"; "/problems?looked=none"

### `/problems/still`
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:149` - members only
  - sends you on to: stay ? "/problems#pain" + std::to_string(id) : std::string("/problems"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/map/map_routes.cpp:163` - members only

### `/profile`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:89`
  - sends you on to: user.empty() ? login_url("/me") : "/me"

### `/proposals`
- **GET** (read the page) - source `site/features/proposals/proposals_routes.cpp:16` - members only
  - sends you on to: "/officials#proposal-" + std::to_string(one->id); "/proposals"; "/proposals?city="+url_encode(key)
  - **group** (top of page)
    - link /proposals 'Proposals /officials#proposal-/proposals?id= All proposals c'
    - link /proposals 'All proposals citycity/proposals/proposals?city= value=""/pr'
    - input name=tag type=checkbox
  - **group** h1: Proposals Races for office are on their own page. Showing pr
    - link /officials 'Races for office are on their own page. Showing proposals fo'
    - link /city/ '. All proposals Start a proposal about thistitletitleNew pro'
    - link /proposals 'All proposals Start a proposal about thistitletitleNew propo'
  - **group** summary: New proposal Title What's being prop
    - form action=/proposals method=post
    - input name=title type=text
    - textarea name=body type=textarea
    - select name=kind type=select
    - input name=unit type=text
    - input name=min type=number
    - input name=max type=number
    - textarea name=options type=textarea
    - input name=open type=checkbox
    - input name=left type=text
    - input name=right type=text
    - input name=city type=text
    - input name=who type=text
    - button id=- 'Publish (function(){var s=document.getElementById('bkind');i'
    - script (inline)
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:139` - members only
  - sends you on to: "/proposals"; "/proposals?city="+url_encode(city_key_chosen); "/proposals?err=dupe"; "/proposals?err=empty"; "/proposals?err=options"; "/proposals?err=pcity"; "/proposals?err=range"
  - builds more via (not followed): parse_form

### `/proposals/option`
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:193` - members only
  - sends you on to: "/proposals"; back_to_proposal(trim_ws(f["next"]), *p); back_to_proposal(trim_ws(f["next"]), *p, full ? "optfull" : "option")
  - builds more via (not followed): parse_form

### `/proposals/react`
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:281` - members only
  - sends you on to: "/proposals"; back_to_proposal(trim_ws(f["next"]), *p)
  - builds more via (not followed): parse_form

### `/proposals/reply`
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:267` - members only
  - sends you on to: "/proposals"; back_to_proposal(trim_ws(f["next"]), *p)
  - builds more via (not followed): parse_form

### `/proposals/vote`
- **POST** (a form posts here) - source `site/features/proposals/proposals_routes.cpp:294` - members only
  - sends you on to: "/proposals"; back_to_proposal(trim_ws(f["next"]), *p); back_to_proposal(trim_ws(f["next"]), *p, "vote")
  - builds more via (not followed): parse_form

### `/push/subscribe`
- **POST** (a form posts here) - source `site/features/push/push_routes.cpp:18` - members only

### `/push/vapid`
- **GET** (read the page) - source `site/features/push/push_routes.cpp:11`

### `/reset`
- **GET** (read the page) - source `site/features/auth/auth_routes.cpp:50`
  - **group** h1: Set a new password New password <input type="password" name=
    - form action=/reset method=post
    - input name=t type=hidden
    - input name=password type=password
    - button id=- 'Set password'
- **POST** (a form posts here) - source `site/features/auth/auth_routes.cpp:168`
  - sends you on to: "/", set_cookie(session.id); "/reset?err=reset"; "/reset?t="+url_encode(t)+"&err=password"; "/reset?t="+url_encode(t)+"&err=short"
  - builds more via (not followed): parse_form

### `/robots.txt`
- **GET** (read the page) - source `site/features/system/system_routes.cpp:13`

### `/search`
- **GET** (read the page) - source `site/features/people/people_routes.cpp:84` - members only
  - **group** h1: Search Find a person <input name="q" id="peopleq" value="" a
    - form action=/search method=get
    - input name=q type=text
    - button id=- 'Search Friends always appear. Everyone else follows their fi'
  - **group** (top of page)  <via person_result_html>
    - link /u/ 'Your profile Accept </form'
    - form action=/friends/accept method=post
    - input name=who type=hidden
    - button id=- 'Accept Request sent <inpu' [submit]
    - form action=/friends method=post
    - input name=who type=hidden
    - input name=next type=hidden
    - button id=- 'Add friend Message <button class="ghost" type="button" disab' [submit]
    - link /mail?with= 'Message Message Profile</'
    - button id=- 'Message Profile sm </span'
    - link /u/ 'Profile sm <'
    - link /u/ ''

### `/settings`
- **GET** (read the page) - source `site/features/settings/settings_routes.cpp:27` - members only
  - **group** h1: Settings Account, privacy, data, and notifications. <a href=
    - link #security 'Security Sessions Map and connections Privacy Block list <a '
    - link #sessions 'Sessions Map and connections Privacy Block list Jukebox <a h'
    - link #map 'Map and connections Privacy Block list Jukebox Identity <a h'
    - link #privacy 'Privacy Block list Jukebox Identity Data Notifi'
    - link #blocks 'Block list Jukebox Identity Data Notifications okpassword<p'
    - link #jukebox 'Jukebox Identity Data Notifications okpassword Password chan'
    - link #identity 'Identity Data Notifications okpassword Password changed. <h2'
    - link #data 'Data Notifications okpassword Password changed. Security <a '
    - link #notifications 'Notifications okpassword Password changed. Security <a class'
  - **group** h2: Security Change password Sessions web
    - link /settings/password 'Change password Sessions web this device Start'
  - **group** h2: Sessions web this device Started · last seen <form method=
    - form action=/settings/revoke method=post
    - input name=sid type=hidden
    - button id=- 'Revoke None. <button class="danger" type="sub' [submit]
    - form action=/settings/logout-all method=post
    - button id=- 'Log out other devices and you can choose to hide it if you w' [submit]
  - **group** h2: Your droplet Your droplet is on every page: walk it with the
    - link /drop/on 'Show my droplet Shown in this browser. Hide my droplet you s'
    - link /drop/off 'Hide my droplet you should be able to turn of NSFW limitatio'
  - **group** h2: Adult videos A link to a video on an adult site shows a "tap
    - link /nsfw/off 'Cover adult videos Cover on in this browser. Show adult vide'
    - link /nsfw/on 'Show adult videos without a cover <input type="checkbox" nam'
    - input name=- type=checkbox
  - **group** h2: Map and connections The Map connects people, groups, cities 
    - link /map 'Map connects people, groups, cities and proposals so you can'
    - form action=/settings/map method=post
    - link /problems 'See your problems and solutions Save map settings Privacy <f'
    - button id=- 'Save map settings Privacy Who can find you</la'
  - **group** h2: Privacy Who can find you One axis, three states. Each level 
    - form action=/settings/privacy method=post
    - input name=findability type=radio
    - input name=findability type=radio
    - input name=findability type=radio
    - input name=group_invite_confirm type=checkbox
    - select name=- type=select
    - table
    - button id=- 'Save privacy Block list member members &middot; not visible '
  - **group** h2: Block list member members &middot; not visible to them No on
    - form action=/settings/unblock method=post
    - input name=who type=hidden
    - button id=- 'Unblock <input name="who" placeholder="account name'
    - form action=/settings/block method=post
    - input name=who type=text
    - button id=- 'Add block its not appropriate checked<h2 id="jukebox"' [submit]
    - input name=reason type=text
  - **group** h2: Jukebox Who else can queue a track on your jukebox <input ty
    - form action=/me/jukebox/policy method=post
    - input name=policy type=radio
    - input name=policy type=radio
    - input name=policy type=radio
    - button id=- 'Save Identity No civic identity is linked to this account. V'
  - **group** h2: Identity No civic identity is linked to this account. Voting (heading only)
  - **group** h2: Data Download your export Export includes username, blurb, p
    - link /export 'Download your export Export includes username, blurb, profil'
    - form action=/settings/delete method=post
    - input name=confirm type=text
    - button id=- 'Delete account Sessions end immediately. Mail you sent is sh'
  - **group** h2: Notifications Friend requests and acc
    - form action=/settings/notify method=post
    - input name=notify_friend type=checkbox
    - input name=notify_message type=checkbox
    - input name=notify_group type=checkbox
    - input name=intro_every_visit type=checkbox
    - button id=- 'Save notifications Settings'
  - **group** h2: Your credibility (preview, only you see this) How many diffe  <via credibility_settings_card> (heading only)
  - **group** (top of page)  <via here_settings_card>
    - input name=here type=radio
  - **group** h2: Dots on profile pages Show yourself as a small round picture  <via here_settings_card>
    - form action=/here/choice method=post
    - input name=back type=hidden
    - button id=- 'Save' [submit]

### `/settings/block`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:349` - members only
  - sends you on to: "/settings"
  - builds more via (not followed): parse_form

### `/settings/delete`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:356` - members only
  - sends you on to: "/", clear_cookie(); "/settings?err=delete"
  - builds more via (not followed): parse_form

### `/settings/hints`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:285` - members only
  - sends you on to: back
  - builds more via (not followed): parse_form

### `/settings/logout-all`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:259` - members only
  - sends you on to: "/settings"

### `/settings/map`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:299` - members only
  - sends you on to: "/settings#map-privacy"
  - builds more via (not followed): parse_form

### `/settings/notify`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:333` - members only
  - sends you on to: "/settings"
  - builds more via (not followed): parse_form

### `/settings/password`
- **GET** (read the page) - source `site/features/settings/settings_routes.cpp:14` - members only
  - **group** h1: Change password &lsaquo; Settings Accou
    - link /settings '&lsaquo; Settings Account <input type="text" name="username"'
    - form action=/settings/password method=post
    - input name=username type=text
    - input name=current type=password
    - input name=next type=password
    - button id=- 'Change password Change password'
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:264` - members only
  - sends you on to: "/settings/password?err=password"; "/settings/password?err=short"; "/settings?ok=password"
  - builds more via (not followed): parse_form

### `/settings/privacy`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:312` - members only
  - sends you on to: "/settings"
  - builds more via (not followed): parse_form

### `/settings/revoke`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:247` - members only
  - sends you on to: "/login", clear_cookie(); "/settings"
  - builds more via (not followed): parse_form

### `/settings/unblock`
- **POST** (a form posts here) - source `site/features/settings/settings_routes.cpp:344` - members only
  - sends you on to: "/settings"
  - builds more via (not followed): parse_form

### `/share`
- **GET** (read the page) - source `site/features/feed/feed_routes.cpp:19` - members only
  - sends you on to: "/feed#share=" + url_encode(out)

### `/signup`
- **GET** (read the page) - source `site/features/auth/auth_routes.cpp:15`
  - **group** h1: Create an account Username is 3–32 characters: letters, numb  <via signup_page>
    - form action=/signup method=post
    - input name=username type=text
    - input name=display_name type=text
    - input name=password type=password
    - input name=email type=email
    - input name=phone type=tel
    - input name=hometown type=text
    - input name=living type=text
    - textarea name=bio type=textarea
    - input name=interests type=text
    - input name=career type=text
    - input name=school type=text
    - button id=- 'Continue'
  - **group** (top of page)  <via google_signin_button_html>
    - script https://accounts.google.com/gsi/client
- **POST** (a form posts here) - source `site/features/auth/auth_routes.cpp:64`
  - sends you on to: "/", set_cookie(r.session->id)
  - **group** h1: Create an account Username is 3–32 characters: letters, numb  <via signup_page>
    - form action=/signup method=post
    - input name=username type=text
    - input name=display_name type=text
    - input name=password type=password
    - input name=email type=email
    - input name=phone type=tel
    - input name=hometown type=text
    - input name=living type=text
    - textarea name=bio type=textarea
    - input name=interests type=text
    - input name=career type=text
    - input name=school type=text
    - button id=- 'Continue'
  - **group** (top of page)  <via google_signin_button_html>
    - script https://accounts.google.com/gsi/client
  - builds more via (not followed): parse_form

### `/styles`
- **GET** (read the page) - source `site/features/styles/styles_routes.cpp:23`
  - **group** h1: Profile styles Looks people made for their profile pages. Us  <via styles_page>
    - link /me#profile-css 'your profile settings . okpublished Published. It's at the t'
    - link /styles 'Newest Most used No styles yet. Be the first: publish yours '
    - link /styles?sort=used 'Most used No styles yet. Be the first: publish yours from yo'
    - link /me#profile-css 'your profile settings . /styles?sort=used&page= Previous <sp'
    - link - 'Previous Page of Next'
    - link - 'Next'
  - **group** (top of page)  <via style_card>
    - iframe /styles//preview
  - **group** h3: by &middot; <form class="inline" me  <via style_card>
    - link /u/ '&middot; <form class="inline" method="POST" action="/styles/'
    - form action=/styles/use method=post
    - input name=id type=hidden
    - button id=- 'Use this style <form class="inline" method="POST" action="/s' [submit]
    - form action=/styles/remove method=post
    - input name=id type=hidden
    - button id=- 'Remove Report <input type="hidden" name="id"' [submit]
  - **group** summary: Report What's wrong with it? (optional) <i  <via style_card>
    - form action=/styles/report method=post
    - input name=id type=hidden
    - input name=reason type=text
    - button id=- 'Send report' [submit]

### `/styles/*`
- **GET** (read the page) - source `site/features/styles/styles_routes.cpp:33`

### `/styles/publish`
- **POST** (a form posts here) - source `site/features/styles/styles_routes.cpp:45` - members only
  - sends you on to: "/me?err=style&reason=" + check.code + "#profile-css"; "/me?err=style&reason=empty#profile-css"; "/me?err=style&reason=limit#profile-css"; "/me?err=style&reason=name#profile-css"; "/styles?ok=published#style-" + std::to_string(*id)
  - builds more via (not followed): parse_form

### `/styles/remove`
- **POST** (a form posts here) - source `site/features/styles/styles_routes.cpp:80` - members only
  - sends you on to: "/styles#style-" + std::to_string(s->id); "/styles?err=gone"; "/styles?ok=removed"
  - builds more via (not followed): parse_form

### `/styles/report`
- **POST** (a form posts here) - source `site/features/styles/styles_routes.cpp:72` - members only
  - sends you on to: "/styles?err=gone"; "/styles?ok=reported#style-" + std::to_string(s->id)
  - builds more via (not followed): parse_form

### `/styles/use`
- **POST** (a form posts here) - source `site/features/styles/styles_routes.cpp:63` - members only
  - sends you on to: "/me?ok=style-used#profile-css"; "/styles?err=gone"
  - builds more via (not followed): parse_form

### `/sw.js`
- **GET** (read the page) - source `site/ui/static_files.cpp:61`

### `/terms`
- **GET** (read the page) - source `site/features/pages/pages_routes.cpp:87`
  - sends you on to: "/control"
  - **group** h1: How this association works LiquidGov.US is a voluntary assoc  <via terms_body>
    - link /control 'who holds control . It is not an office. The three laws · Wh'
    - link /laws 'The three laws · What we keep'
    - link /privacy 'What we keep'

### `/theme/dark`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:91`
  - sends you on to: back, extra

### `/theme/light`
- **GET** (read the page) - source `site/features/assets/assets_routes.cpp:91`
  - sends you on to: back, extra

### `/u/*`
- **GET** (read the page) - source `site/features/jukebox/jukebox_routes.cpp:261`
- **GET** (read the page) - source `site/features/profile/profile_routes.cpp:115`
  - **group** (top of page)
    - link /c/ 'Edit profile /u/plain viewplain1/u/based on<p class="mu'
    - link /me 'Edit profile /u/plain viewplain1/u/based on Style: </'
    - link /styles#style- 'by Plain view. Show 's style <p class="mut'
    - link /u/ 'Plain view. Show 's style <p class="muted lg-plain-view" sty'
    - link - 'Show 's style Plain view <div class="lg-profile-outer"'
    - link ?plain=1 'Plain view <s'
  - **group** h1: · from · · Standing
    - table
    - link /mail?with= 'Message <form class="inline" method="POST" action="/friends/'
    - form action=/friends/unfriend method=post
    - input name=who type=hidden
    - button id=- 'Unfriend Send friend request Play' [submit]
    - form action=/friends method=post
    - input name=who type=hidden
    - button id=- 'Send friend request Play my Juketube when someone opens my p'
    - script (inline)
  - **group** h2: Posts entry entries (heading only)
  - **group** h2: Earlier portraits <img src="/portrait-history//?v=" alt="" w  <via portrait_history_public_html>
    - img /portrait-history//?v=
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:336` - members only
  - sends you on to: "/"; "/u/"+who; "/u/"+who+"?err=denied"; "/u/"+who+"?err=full"; "/u/"+who+"?err=pending"; "/u/"+who+"?err=url"
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:367` - members only
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:389` - members only
  - builds more via (not followed): parse_form
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:425` - members only
- **POST** (a form posts here) - source `site/features/jukebox/jukebox_routes.cpp:439` - members only

### `/world`
- **GET** (read the page) - source `site/features/live/live_routes.cpp:8`
  - sends you on to: "/commons/"

