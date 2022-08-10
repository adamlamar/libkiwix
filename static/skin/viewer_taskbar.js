function htmlDecode(input) {
    var doc = new DOMParser().parseFromString(input, "text/html");
    return doc.documentElement.textContent;
}

function setupAutoHidingOfTheToolbar() {
  let lastScrollTop = 0;
  const delta = 5;
  let didScroll = false;
  const kiwixToolBar = document.querySelector('#kiwixtoolbar');

  window.addEventListener('scroll', () => {
    didScroll = true;
  });

  setInterval(function() {
    if (didScroll) {
      hasScrolled();
      didScroll = false;
    }
  }, 250);

  function hasScrolled() {
    const st = document.documentElement.scrollTop || document.body.scrollTop;
    if (Math.abs(lastScrollTop - st) <= delta)
        return;

    if (st > lastScrollTop) {
        kiwixToolBar.style.top = '-100%';
    } else {
        kiwixToolBar.style.top = '0';
    }

    lastScrollTop = st;
  }

}

document.addEventListener('DOMContentLoaded', function () {
  const p = location.pathname;
  const root = p.slice(0, p.length - '/viewer'.length);

  const bookName = location.hash.slice(1).split('/')[0];

  const kiwixSearchBox = document.querySelector('#kiwixsearchbox');
  const kiwixSearchFormWrapper = document.querySelector('.kiwix_searchform');

  const autoCompleteJS = new autoComplete(
    {
      selector: "#kiwixsearchbox",
      placeHolder: kiwixSearchBox.title,
      threshold: 1,
      debounce: 300,
      data : {
        src: async (query) => {
          try {
            // Fetch Data from external Source
            const source = await fetch(`${root}/suggest?content=${encodeURIComponent(bookName)}&term=${encodeURIComponent(query)}`);
            const data = await source.json();
            return data;
          } catch (error) {
            return error;
          }
        },
        keys: ['label'],
      },
      submit: true,
      searchEngine: (query, record) => {
        // We accept all records
        return true;
      },
      resultsList: {
        noResults: true,
        // We must display 10 results (requested) + 1 potential link to do a full text search.
        maxResults: 11,
      },
      resultItem: {
        element: (item, data) => {
          let searchLink;
          if (data.value.kind == "path") {
            searchLink = `${root}/${bookName}/${htmlDecode(data.value.path)}`;
          } else {
            searchLink = `${root}/search?content=${encodeURIComponent(bookName)}&pattern=${encodeURIComponent(htmlDecode(data.value.value))}`;
          }
          item.innerHTML = `<a class="suggest" href="javascript:gotoUrl('${searchLink}')">${htmlDecode(data.value.label)}</a>`;
        },
        highlight: "autoComplete_highlight",
        selected: "autoComplete_selected"
      }
    }
  );

  document.querySelector('#kiwixsearchform').addEventListener('submit', function(event) {
    try {
      const selectedElem = document.querySelector('.autoComplete_selected > a');
      if (selectedElem) {
        event.preventDefault();
        selectedElem.click();
      }
    } catch (err) {}
  });

  kiwixSearchBox.addEventListener('focus', () => {
      kiwixSearchFormWrapper.classList.add('full_width');
      document.querySelector('label[for="kiwix_button_show_toggle"]').classList.add('searching');
      document.querySelector('.kiwix_button_cont').classList.add('searching');
  });
  kiwixSearchBox.addEventListener('blur', () => {
      kiwixSearchFormWrapper.classList.remove('full_width');
      document.querySelector('label[for="kiwix_button_show_toggle"]').classList.remove('searching');
      document.querySelector('.kiwix_button_cont').classList.remove('searching');
  });

  // cybook hack
  if (navigator.userAgent.indexOf("bookeen/cybook") != -1) {
      document.querySelector('html').classList.add('cybook');
  }

  if (document.body.clientWidth < 520) {
    setupAutoHidingOfTheToolbar();
  }
});
