if (!window.__tg__webview_set) {
    window.__tg__webview_set = true;
    (function () {
        const DEBUG = $DEBUG$;

        let prevented = false;
        let awaitingResponse = false;
        let touchElement = null;
        let mutatedWhileTouch = false;
        let whiletouchstart = false, whiletouchmove = false;
        document.addEventListener('touchstart', e => {
            touchElement = e.target;
            awaitingResponse = true;
            whiletouchstart = true;
        }, true);
        document.addEventListener('touchstart', e => {
            whiletouchstart = false;
        }, false);
        const atLeft = e => !e || e == document || e.scrollLeft <= 0 && atLeft(e.parentNode);
        const atTop = e => !e || e == document || e.scrollTop <= 0 && atTop(e.parentNode);
        document.addEventListener('touchmove', e => {
            whiletouchstart = false;
            whiletouchmove = true;
            if (awaitingResponse) {
                setTimeout(() => {
                    if (awaitingResponse) {
                        if (window.TelegramWebviewProxy) {
                            const allowScrollX = !prevented && atLeft(e.target) && (!window.visualViewport || window.visualViewport.offsetLeft == 0) && !mutatedWhileTouch;
                            const allowScrollY = !prevented && atTop(e.target)  && (!window.visualViewport || window.visualViewport.offsetTop == 0)  && !mutatedWhileTouch;
                            if (DEBUG) {
                                console.log('tgbrowser allowScroll sent after "touchmove": x=' + allowScrollX + ' y=' + allowScrollY, { e, prevented, mutatedWhileTouch });
                            }
                            window.TelegramWebviewProxy.postEvent('web_app_allow_scroll', JSON.stringify([ allowScrollX, allowScrollY ]));
                        }
                        prevented = false;
                        awaitingResponse = false;
                    }
                    mutatedWhileTouch = false;
                }, 16);
            }
        }, true);
        document.addEventListener('touchmove', e => {
            whiletouchmove = false;
        }, false);
        document.addEventListener('scroll', e => {
            if (!e.target) return;
            const allowScrollX = e.target.scrollLeft == 0 && (!window.visualViewport || window.visualViewport.offsetLeft == 0) && !prevented && !mutatedWhileTouch;
            const allowScrollY = e.target.scrollTop == 0  && (!window.visualViewport || window.visualViewport.offsetTop == 0)  && !prevented && !mutatedWhileTouch;
            if (DEBUG) {
                console.log('tgbrowser scroll on' + e.target + ' scrollLeft=' + e.target.scrollLeft + ' scrollTop=' + e.target.scrollTop);
            }
            if (awaitingResponse) {
                if (window.TelegramWebviewProxy) {
                    if (DEBUG) {
                        console.log('tgbrowser allowScroll sent after "scroll": x=' + allowScrollX + ' y=' + allowScrollY, { e, prevented, mutatedWhileTouch, scrollLeft: e.target.scrollLeft, scrollTop: e.target.scrollTop });
                    }
                    window.TelegramWebviewProxy.postEvent('web_app_allow_scroll', JSON.stringify([allowScrollX, allowScrollY]));
                }
                awaitingResponse = false;
            }
            prevented = false;
            mutatedWhileTouch = false;
        }, true);
        if (TouchEvent) {
            const originalPreventDefault = TouchEvent.prototype.preventDefault;
            TouchEvent.prototype.preventDefault = function () {
                prevented = true;
                originalPreventDefault.call(this);
            };
            const originalStopPropagation = TouchEvent.prototype.stopPropagation;
            TouchEvent.prototype.stopPropagation = function () {
                if (this.type === 'touchmove') {
                    whiletouchmove = false;
                } else if (this.type === 'touchstart') {
                    whiletouchstart = false;
                }
                originalStopPropagation.call(this);
            };
        }
        const isParentOf = (e, p) => {
            if (!e || !p) return false;
            if (e == p) return true;
            return isParentOf(e.parentElement, p);
        }
        new MutationObserver(mutationList => {
            const isTouchElement = touchElement && !![...(mutationList||[])]
                .filter(r => r && (r.attributeName === 'style' || r.attributeName === 'class'))
                .map(r => r.target)
                .filter(e => !!e && e != document.body && e != document.documentElement)
                .find(e => isParentOf(touchElement, e));
            if (isTouchElement) {
                if (DEBUG) {
                    console.log('tgbrowser mutation detected', mutationList);
                }
                mutatedWhileTouch = true;
            }
        }).observe(document, { attributes: true, childList: true, subtree: true });
    })();
};