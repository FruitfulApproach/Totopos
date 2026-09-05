// Editor interop for the AbCatDC type-expression editor.
window.abEditor = {
    attach: function (id, dotnetRef, submitOnEnter) {
        const el = document.getElementById(id);
        if (!el || el.__abAttached) return;
        el.__abAttached = true;
        el.__abRef = dotnetRef;
        el.__abPopupOpen = false;
        el.__abSubmit = !!submitOnEnter;
        const closePopup = function () {
            if (el.__abPopupOpen)
                dotnetRef.invokeMethodAsync('OnEditorKey', 'CloseOnly', el.selectionStart, el.value);
        };
        el.addEventListener('keydown', function (e) {
            if (e.ctrlKey && (e.code === 'Space' || e.key === ' ')) {
                e.preventDefault();
                dotnetRef.invokeMethodAsync('OnEditorKey', 'CtrlSpace', el.selectionStart, el.value);
                return;
            }
            if (el.__abPopupOpen && ['ArrowUp', 'ArrowDown', 'Enter', 'Tab', 'Escape'].includes(e.key)) {
                e.preventDefault();
                dotnetRef.invokeMethodAsync('OnEditorKey', e.key, el.selectionStart, el.value);
                return;
            }
            if (el.__abSubmit && e.key === 'Enter' && !e.shiftKey) {
                e.preventDefault();
                dotnetRef.invokeMethodAsync('OnEditorKey', 'Submit', el.selectionStart, el.value);
                return;
            }
            // caret moved away from the completion context — close, key acts normally
            if (el.__abPopupOpen && ['ArrowLeft', 'ArrowRight', 'Home', 'End', 'PageUp', 'PageDown'].includes(e.key))
                closePopup();
        });
        el.addEventListener('click', closePopup);
        el.addEventListener('blur', closePopup);
    },

    setPopupOpen: function (id, open) {
        const el = document.getElementById(id);
        if (el) el.__abPopupOpen = open;
    },

    getState: function (id) {
        const el = document.getElementById(id);
        return el
            ? { text: el.value, cursor: el.selectionStart ?? el.value.length }
            : { text: '', cursor: 0 };
    },

    replaceRange: function (id, start, end, insert) {
        const el = document.getElementById(id);
        if (!el) return { text: '', cursor: 0 };
        const before = el.value.substring(0, start);
        const after = el.value.substring(end);
        el.value = before + insert + after;
        const pos = start + insert.length;
        el.selectionStart = el.selectionEnd = pos;
        el.focus();
        return { text: el.value, cursor: pos };
    },

    insertAtCursor: function (id, insert) {
        const el = document.getElementById(id);
        if (!el) return { text: '', cursor: 0 };
        const s = el.selectionStart ?? el.value.length;
        const e = el.selectionEnd ?? el.value.length;
        return this.replaceRange(id, s, e, insert);
    },

    setValue: function (id, text) {
        const el = document.getElementById(id);
        if (!el) return { text: '', cursor: 0 };
        el.value = text;
        el.selectionStart = el.selectionEnd = text.length;
        el.focus();
        return { text: el.value, cursor: text.length };
    },

    theme: function (dark) {
        document.documentElement.setAttribute('data-ab-theme', dark ? 'dark' : 'light');
    },

    // Read the #q=... fragment out of a same-origin quiver iframe. Quiver
    // writes its diagram into the URL on Save (Ctrl+S), so this closes the
    // round trip without copy/paste.
    readQuiverFrame: function (id) {
        try {
            const el = document.getElementById(id);
            const hash = (el && el.contentWindow && el.contentWindow.location.hash) || '';
            const m = hash.match(/q=([^&]*)/);
            return m ? m[1] : '';
        } catch { return ''; }
    },

    // Full-screen diagram zoom, mounted on <body> so no transformed ancestor
    // can trap the fixed positioning or stacking context. When editRef/editArg
    // are given, a hover edit button invokes .NET ZoomEdit(editArg) to open the
    // real quiver editor for that sketch.
    showZoom: function (src, editRef, editArg) {
        if (document.getElementById('ab-zoom-ov')) return;
        const ov = document.createElement('div');
        ov.id = 'ab-zoom-ov';
        ov.className = 'ab-zoom-overlay';
        const fr = document.createElement('iframe');
        fr.className = 'ab-zoom-frame';
        fr.title = 'diagram — enlarged';
        fr.addEventListener('load', function () { ov.classList.add('ab-loaded'); });
        fr.src = src;
        const btn = document.createElement('button');
        btn.className = 'ab-zoom-close';
        btn.title = 'Close';
        btn.textContent = '×';
        const close = function () {
            ov.remove();
            document.removeEventListener('keydown', onKey);
        };
        const onKey = function (e) { if (e.key === 'Escape') close(); };
        ov.addEventListener('click', function (e) { if (e.target === ov) close(); });
        ov.addEventListener('dblclick', function (e) { if (e.target === ov) close(); });
        btn.addEventListener('click', close);
        document.addEventListener('keydown', onKey);
        ov.appendChild(fr);
        ov.appendChild(btn);
        if (editRef) {
            const ed = document.createElement('button');
            ed.className = 'ab-zoom-edit';
            ed.title = 'Edit this diagram';
            ed.innerHTML = '<i class="fa-solid fa-pencil"></i>';
            ed.addEventListener('click', function () {
                close();
                editRef.invokeMethodAsync('ZoomEdit', editArg);
            });
            ov.appendChild(ed);
        }
        document.body.appendChild(ov);
    },

    // Crop a PNG capture of the whole web view (base64) to the diagram drawn
    // in the quiver frame `id`: the union of the cells' boxes, padded — or the
    // frame itself when they cannot be read. Returns a data URL, scaled down
    // to at most maxWidth device pixels wide.
    cropSnapshot: function (b64, id, maxWidth) {
        return new Promise(function (resolve) {
            var el = document.getElementById(id);
            if (!el || !b64) { resolve(''); return; }
            var fr = el.getBoundingClientRect();
            var box = null;
            try {
                var doc = el.contentDocument;
                var cells = doc ? doc.querySelectorAll('.vertex, .edge, .arrow, .cell') : [];
                cells.forEach(function (c) {
                    var b = c.getBoundingClientRect();
                    if (b.width === 0 && b.height === 0) return;
                    if (!box) box = { l: b.left, t: b.top, r: b.right, b: b.bottom };
                    else { box.l = Math.min(box.l, b.left); box.t = Math.min(box.t, b.top); box.r = Math.max(box.r, b.right); box.b = Math.max(box.b, b.bottom); }
                });
            } catch (e) { box = null; }
            var pad = 18;
            var x, y, w, h;
            if (box) { x = fr.left + box.l - pad; y = fr.top + box.t - pad; w = box.r - box.l + 2 * pad; h = box.b - box.t + 2 * pad; }
            else { x = fr.left; y = fr.top; w = fr.width; h = fr.height; }
            x = Math.max(0, x); y = Math.max(0, y);
            var dpr = window.devicePixelRatio || 1;
            var img = new Image();
            img.onload = function () {
                var sx = x * dpr, sy = y * dpr, sw = Math.max(1, w * dpr), sh = Math.max(1, h * dpr);
                var scale = Math.min(1, (maxWidth || 900) / sw);
                var c = document.createElement('canvas');
                c.width = Math.max(1, Math.round(sw * scale)); c.height = Math.max(1, Math.round(sh * scale));
                var ctx = c.getContext('2d');
                ctx.fillStyle = '#ffffff'; ctx.fillRect(0, 0, c.width, c.height);
                ctx.drawImage(img, sx, sy, sw, sh, 0, 0, c.width, c.height);
                resolve(c.toDataURL('image/png'));
            };
            img.onerror = function () { resolve(''); };
            img.src = 'data:image/png;base64,' + b64;
        });
    },

    // A "teach me" arrow: bounces toward the element `targetId` with a note,
    // until the element is clicked, teachOff() is called, or the timeout.
    teach: function (targetId, text, ms) {
        window.abEditor.teachOff();
        var target = document.getElementById(targetId);
        if (!target) return;
        var el = document.createElement('div');
        el.className = 'ab-teach';
        el.innerHTML = '<div class="ab-teach-note"></div><div class="ab-teach-arrow">&#10148;</div>';
        el.querySelector('.ab-teach-note').textContent = text || '';
        document.body.appendChild(el);
        var place = function () {
            var r = target.getBoundingClientRect();
            el.style.top = (r.top + r.height / 2) + 'px';
            el.style.left = r.left + 'px';
        };
        place();
        var onResize = function () { place(); };
        window.addEventListener('resize', onResize);
        var off = function () { window.abEditor.teachOff(); };
        target.addEventListener('click', off, { once: true });
        window.__abTeach = { el: el, onResize: onResize, timer: setTimeout(off, ms || 15000) };
    },
    teachOff: function () {
        var t = window.__abTeach;
        if (!t) return;
        window.__abTeach = null;
        try { clearTimeout(t.timer); window.removeEventListener('resize', t.onResize); t.el.remove(); } catch (e) { }
    },

    loadItem: function (key) {
        try { return localStorage.getItem(key); } catch { return null; }
    },

    saveItem: function (key, json) {
        try { localStorage.setItem(key, json); } catch { }
    },

    loadSettings: function () {
        try { return localStorage.getItem('abcatdc-settings'); } catch { return null; }
    },

    saveSettings: function (json) {
        try { localStorage.setItem('abcatdc-settings', json); } catch { }
    }
};
