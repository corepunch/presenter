(() => {
  const status = document.getElementById('copy-status');
  let statusTimer;
  function announce(message) {
    clearTimeout(statusTimer);
    status.textContent = message;
    statusTimer = setTimeout(() => { status.textContent = ''; }, 5000);
  }

  document.querySelectorAll('[data-copy]').forEach(button => {
    button.addEventListener('click', async () => {
      const target = document.getElementById(button.dataset.copy);
      const value = target instanceof HTMLTextAreaElement ? target.value : target.textContent;
      try {
        await navigator.clipboard.writeText(value.trim());
        announce(button.dataset.copy === 'agent-prompt' ? 'Prompt copied. Paste it into your AI agent.' : 'Command copied.');
      } catch {
        // Keep the original text selectable when browser permissions block copying.
        if (target instanceof HTMLTextAreaElement) {
          target.focus();
          target.select();
        } else {
          const selection = window.getSelection();
          const range = document.createRange();
          range.selectNodeContents(target);
          selection.removeAllRanges();
          selection.addRange(range);
        }
        announce('Copy was blocked. Text selected — press ⌘C or Ctrl+C to copy.');
      }
    });
  });
})();
