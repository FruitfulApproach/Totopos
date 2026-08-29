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
