// Shows the page's module script under the plots, so each example reads as its own docs.
const script = document.querySelector('script[type="module"]');
const pre = document.createElement('pre');
pre.className = 'source';
pre.textContent = script.textContent.replace(/^\n/, '').replace(/\s+$/, '');
document.querySelector('main').append(Object.assign(document.createElement('h3'), { textContent: 'Source' }), pre);
