/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 * SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "lionexplorertabpage.h"

#include "lionexplorer_generalsettings.h"
#include "lionexplorerviewcontainer.h"

#include <QFrame>
#include <QGridLayout>
#include <QStyle>
#include <QVariantAnimation>

namespace
{
void resetSplitterSizes(QSplitter *splitter)
{
    splitter->setSizes({0, 0});
}
}

Lion ExplorerTabPage::Lion ExplorerTabPage(const QUrl &primaryUrl, const QUrl &secondaryUrl, QWidget *parent)
    : QWidget(parent)
    , m_expandingContainer{nullptr}
    , m_primaryViewActive(true)
    , m_splitViewEnabled(false)
    , m_active(true)
    , m_splitterLastPosition(0)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);

    m_splitter = new Lion ExplorerTabPageSplitter(Qt::Horizontal, this);
    m_splitter->setChildrenCollapsible(false);
    connect(m_splitter, &QSplitter::splitterMoved, this, &Lion ExplorerTabPage::splitterMoved);
    layout->addWidget(m_splitter, 2, 0, 1, 2);
    layout->setRowStretch(2, 1);

    // Create a new primary view
    m_primaryViewContainer = createViewContainer(primaryUrl);
    connect(m_primaryViewContainer->view(), &Lion ExplorerView::urlChanged, this, &Lion ExplorerTabPage::activeViewUrlChanged);
    connect(m_primaryViewContainer->view(), &Lion ExplorerView::redirection, this, &Lion ExplorerTabPage::slotViewUrlRedirection);

    m_splitter->addWidget(m_primaryViewContainer);
    m_primaryViewContainer->show();

    if (secondaryUrl.isValid() || GeneralSettings::splitView()) {
        // Provide a secondary view, if the given secondary url is valid or if the
        // startup settings are set this way (use the url of the primary view).
        m_splitViewEnabled = true;
        const QUrl &url = secondaryUrl.isValid() ? secondaryUrl : primaryUrl;
        m_secondaryViewContainer = createViewContainer(url);
        connect(m_secondaryViewContainer->view(), &Lion ExplorerView::redirection, this, &Lion ExplorerTabPage::slotViewUrlRedirection);
        m_splitter->addWidget(m_secondaryViewContainer);
        m_secondaryViewContainer->show();
    }

    // Lion ExplorerView::setActive(true) calls setFocus() then emits activated().
    // activated() is connected to slotViewActivated() which toggles
    // m_primaryViewActive — correct for user-initiated pane switches, but
    // wrong here during construction. Disconnect to prevent the spurious toggle.
    disconnectViewActivatedSignals();
    m_primaryViewContainer->setActive(true);
    connectViewActivatedSignals();
}

bool Lion ExplorerTabPage::primaryViewActive() const
{
    return m_primaryViewActive;
}

bool Lion ExplorerTabPage::splitViewEnabled() const
{
    return m_splitViewEnabled;
}

void Lion ExplorerTabPage::splitterMoved(int pos)
{
    m_splitterLastPosition = pos;
}

void Lion ExplorerTabPage::setSplitViewEnabled(bool enabled, Animated animated, const QUrl &secondaryUrl)
{
    if (m_splitViewEnabled == enabled) {
        return;
    }

    m_splitViewEnabled = enabled;
    if (animated == WithAnimation
        && (style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, this) < 1 || GlobalConfig::animationDurationFactor() <= 0.0)) {
        animated = WithoutAnimation;
    }
    if (m_expandViewAnimation) {
        m_expandViewAnimation->stop(); // deletes because of QAbstractAnimation::DeleteWhenStopped.
        if (animated == WithoutAnimation) {
            slotAnimationFinished();
        }
    }

    if (enabled) {
        QList<int> splitterSizes = m_splitter->sizes();
        const QUrl &url = (secondaryUrl.isEmpty()) ? m_primaryViewContainer->url() : secondaryUrl;
        m_secondaryViewContainer = createViewContainer(url);

        auto secondaryNavigator = m_navigatorsWidget->secondaryUrlNavigator();
        if (!secondaryNavigator) {
            m_navigatorsWidget->createSecondaryUrlNavigator();
            secondaryNavigator = m_navigatorsWidget->secondaryUrlNavigator();
        }
        m_secondaryViewContainer->connectUrlNavigator(secondaryNavigator);
        connect(m_secondaryViewContainer->view(), &Lion ExplorerView::redirection, this, &Lion ExplorerTabPage::slotViewUrlRedirection);
        m_navigatorsWidget->setSecondaryNavigatorVisible(true);
        m_navigatorsWidget->followViewContainersGeometry(m_primaryViewContainer, m_secondaryViewContainer);

        m_splitter->addWidget(m_secondaryViewContainer);
        m_secondaryViewContainer->setActive(true);

        if (animated == WithAnimation) {
            m_secondaryViewContainer->setMinimumWidth(1);
            splitterSizes.append(1);
            m_splitter->setSizes(splitterSizes);
            startExpandViewAnimation(m_secondaryViewContainer);
        } else {
            resetSplitterSizes(m_splitter);
        }
        m_secondaryViewContainer->show();
    } else {
        m_navigatorsWidget->setSecondaryNavigatorVisible(false);
        m_secondaryViewContainer->disconnectUrlNavigator();
        disconnect(m_secondaryViewContainer->view(), &Lion ExplorerView::redirection, this, &Lion ExplorerTabPage::slotViewUrlRedirection);

        Lion ExplorerViewContainer *view;

        auto swapActiveView = [this]() {
            m_primaryViewContainer->disconnectUrlNavigator();
            m_secondaryViewContainer->connectUrlNavigator(m_navigatorsWidget->primaryUrlNavigator());

            // If the primary view is active, we have to swap the pointers
            // because the secondary view will be the new primary view.
            std::swap(m_primaryViewContainer, m_secondaryViewContainer);
            m_primaryViewActive = !m_primaryViewActive;
        };
        using Choice = GeneralSettings::EnumCloseSplitViewChoice;
        switch (GeneralSettings::closeSplitViewChoice()) {
        case Choice::ActiveView:
            view = activeViewContainer();
            if (m_primaryViewActive) {
                swapActiveView();
            }
            break;
        case Choice::InactiveView:
            view = m_primaryViewActive ? m_secondaryViewContainer : m_primaryViewContainer;
            if (!m_primaryViewActive) {
                swapActiveView();
            }
            break;
        case Choice::RightView:
            view = m_secondaryViewContainer;
            if (!m_primaryViewActive) {
                swapActiveView();
            }
            break;
        default:
            Q_UNREACHABLE();
        }
        m_primaryViewContainer->setActive(true);
        m_navigatorsWidget->followViewContainersGeometry(m_primaryViewContainer, nullptr);

        if (animated == WithoutAnimation) {
            view->close();
            view->deleteLater();
        } else {
            // Kill it but keep it as a zombie for the closing animation.
            m_secondaryViewContainer = nullptr;
            view->blockSignals(true);
            view->view()->blockSignals(true);
            view->setDisabled(true);
            startExpandViewAnimation(m_primaryViewContainer);
        }

        m_primaryViewContainer->slotSplitTabDisabled();
    }
}

Lion ExplorerViewContainer *Lion ExplorerTabPage::primaryViewContainer() const
{
    return m_primaryViewContainer;
}

Lion ExplorerViewContainer *Lion ExplorerTabPage::secondaryViewContainer() const
{
    return m_secondaryViewContainer;
}

Lion ExplorerViewContainer *Lion ExplorerTabPage::activeViewContainer() const
{
    return m_primaryViewActive ? m_primaryViewContainer : m_secondaryViewContainer;
}

Lion ExplorerViewContainer *Lion ExplorerTabPage::inactiveViewContainer() const
{
    if (!splitViewEnabled()) {
        return nullptr;
    }

    return primaryViewActive() ? secondaryViewContainer() : primaryViewContainer();
}

KFileItemList Lion ExplorerTabPage::selectedItems() const
{
    KFileItemList items = m_primaryViewContainer->view()->selectedItems();
    if (m_splitViewEnabled) {
        items += m_secondaryViewContainer->view()->selectedItems();
    }
    return items;
}

int Lion ExplorerTabPage::selectedItemsCount() const
{
    int selectedItemsCount = m_primaryViewContainer->view()->selectedItemsCount();
    if (m_splitViewEnabled) {
        selectedItemsCount += m_secondaryViewContainer->view()->selectedItemsCount();
    }
    return selectedItemsCount;
}

void Lion ExplorerTabPage::connectNavigators(Lion ExplorerNavigatorsWidgetAction *navigatorsWidget)
{
    insertNavigatorsWidget(navigatorsWidget);
    m_navigatorsWidget = navigatorsWidget;
    auto primaryNavigator = navigatorsWidget->primaryUrlNavigator();
    m_primaryViewContainer->connectUrlNavigator(primaryNavigator);
    if (m_splitViewEnabled) {
        auto secondaryNavigator = navigatorsWidget->secondaryUrlNavigator();
        m_secondaryViewContainer->connectUrlNavigator(secondaryNavigator);
    }
    m_navigatorsWidget->followViewContainersGeometry(m_primaryViewContainer, m_secondaryViewContainer);
}

void Lion ExplorerTabPage::disconnectNavigators()
{
    m_navigatorsWidget = nullptr;
    m_navigatorSeparator = nullptr;
    m_primaryViewContainer->disconnectUrlNavigator();
    if (m_splitViewEnabled) {
        m_secondaryViewContainer->disconnectUrlNavigator();
    }
}

void Lion ExplorerTabPage::insertNavigatorsWidget(Lion ExplorerNavigatorsWidgetAction *navigatorsWidget)
{
    QGridLayout *gridLayout = static_cast<QGridLayout *>(layout());
    if (navigatorsWidget->isInToolbar()) {
        if (m_navigatorSeparator) {
            m_navigatorSeparator->setFrameStyle(QFrame::NoFrame);
            gridLayout->removeWidget(m_navigatorSeparator.get());
        }
        gridLayout->setRowMinimumHeight(0, 0);
    } else {
        // We set a row minimum height, so the height does not visibly change whenever
        // navigatorsWidget is inserted which happens every time the current tab is changed.
        gridLayout->setRowMinimumHeight(0, navigatorsWidget->primaryUrlNavigator()->height());
        gridLayout->setRowMinimumHeight(1, 1);

        gridLayout->addWidget(navigatorsWidget->requestWidget(this), 0, 0);
        if (!m_navigatorSeparator) {
            m_navigatorSeparator = std::make_unique<QFrame>(this);
        }
        m_navigatorSeparator->setFrameStyle(QFrame::HLine);
        m_navigatorSeparator->setFixedHeight(1);
        m_navigatorSeparator->setContentsMargins(0, 0, 0, 0);
        gridLayout->addWidget(m_navigatorSeparator.get(), 1, 0);
    }
}

void Lion ExplorerTabPage::markUrlsAsSelected(const QList<QUrl> &urls)
{
    m_primaryViewContainer->view()->markUrlsAsSelected(urls);
    if (m_splitViewEnabled) {
        m_secondaryViewContainer->view()->markUrlsAsSelected(urls);
    }
}

void Lion ExplorerTabPage::markUrlAsCurrent(const QUrl &url)
{
    m_primaryViewContainer->view()->markUrlAsCurrent(url);
    if (m_splitViewEnabled) {
        m_secondaryViewContainer->view()->markUrlAsCurrent(url);
    }
}

void Lion ExplorerTabPage::refreshViews()
{
    m_primaryViewContainer->readSettings();
    if (m_splitViewEnabled) {
        m_secondaryViewContainer->readSettings();
    }
}

QByteArray Lion ExplorerTabPage::saveState() const
{
    QByteArray state;
    QDataStream stream(&state, QIODevice::WriteOnly);

    stream << quint32(3); // Tab state version

    stream << m_splitViewEnabled;

    stream << m_primaryViewContainer->url();
    stream << m_primaryViewContainer->urlNavigatorInternalWithHistory()->isUrlEditable();
    m_primaryViewContainer->view()->saveState(stream);

    if (m_splitViewEnabled) {
        stream << m_secondaryViewContainer->url();
        stream << m_secondaryViewContainer->urlNavigatorInternalWithHistory()->isUrlEditable();
        m_secondaryViewContainer->view()->saveState(stream);
    }

    stream << m_primaryViewActive;
    stream << m_splitter->saveState();
    stream << m_splitterLastPosition;

    if (!m_customLabel.isEmpty()) {
        stream << m_customLabel;
    }

    return state;
}

void Lion ExplorerTabPage::restoreState(const QByteArray &state)
{
    if (state.isEmpty()) {
        return;
    }

    QByteArray sd = state;
    QDataStream stream(&sd, QIODevice::ReadOnly);

    // Read the version number of the tab state and check if the version is supported.
    quint32 version = 0;
    stream >> version;
    if (version < 2 || version > 3) {
        // The version of the tab state isn't supported, we can't restore it.
        return;
    }

    bool isSplitViewEnabled = false;
    stream >> isSplitViewEnabled;
    setSplitViewEnabled(isSplitViewEnabled, WithoutAnimation);

    QUrl primaryUrl;
    stream >> primaryUrl;
    m_primaryViewContainer->setUrl(primaryUrl);
    bool primaryUrlEditable;
    stream >> primaryUrlEditable;
    m_primaryViewContainer->urlNavigatorInternalWithHistory()->setUrlEditable(primaryUrlEditable);
    m_primaryViewContainer->view()->restoreState(stream);

    if (isSplitViewEnabled) {
        QUrl secondaryUrl;
        stream >> secondaryUrl;
        m_secondaryViewContainer->setUrl(secondaryUrl);
        bool secondaryUrlEditable;
        stream >> secondaryUrlEditable;
        m_secondaryViewContainer->urlNavigatorInternalWithHistory()->setUrlEditable(secondaryUrlEditable);
        m_secondaryViewContainer->view()->restoreState(stream);
    }

    stream >> m_primaryViewActive;
    // Lion ExplorerView::setActive(true) calls setFocus() then emits activated().
    // activated() is connected to slotViewActivated() which toggles
    // m_primaryViewActive — correct for user-initiated pane switches, but
    // wrong here during session restore. Disconnect to prevent the spurious toggle.
    disconnectViewActivatedSignals();
    if (m_primaryViewActive) {
        if (m_splitViewEnabled) {
            m_secondaryViewContainer->setActive(false);
        }
        m_primaryViewContainer->setActive(true);
    } else {
        Q_ASSERT(m_splitViewEnabled);
        m_primaryViewContainer->setActive(false);
        m_secondaryViewContainer->setActive(true);
    }
    connectViewActivatedSignals();

    QByteArray splitterState;
    stream >> splitterState;
    m_splitter->restoreState(splitterState);
    if (version >= 3) {
        stream >> m_splitterLastPosition;
    }

    if (!stream.atEnd()) {
        QString tabTitle;
        stream >> tabTitle;
        setCustomLabel(tabTitle);
    }
}

void Lion ExplorerTabPage::setActive(bool active)
{
    if (active) {
        m_active = active;
    } else {
        // we should bypass changing active view in split mode
        m_active = !m_splitViewEnabled;
    }
    // Lion ExplorerView::setActive(true) calls setFocus() then emits activated().
    // activated() is connected to slotViewActivated() which toggles
    // m_primaryViewActive — correct for user-initiated pane switches, but
    // wrong here during tab switch. Disconnect to prevent the spurious toggle.
    disconnectViewActivatedSignals();
    if (active && m_splitViewEnabled) {
        inactiveViewContainer()->setActive(false);
    }
    activeViewContainer()->setActive(active);
    connectViewActivatedSignals();
}

void Lion ExplorerTabPage::setCustomLabel(const QString &label)
{
    m_customLabel = label;
}

QString Lion ExplorerTabPage::customLabel() const
{
    return m_customLabel;
}

void Lion ExplorerTabPage::slotAnimationFinished()
{
    for (int i = 0; i < m_splitter->count(); ++i) {
        QWidget *viewContainer = m_splitter->widget(i);
        if (viewContainer != m_primaryViewContainer && viewContainer != m_secondaryViewContainer) {
            viewContainer->close();
            viewContainer->deleteLater();
        }
    }
    for (int i = 0; i < m_splitter->count(); ++i) {
        QWidget *viewContainer = m_splitter->widget(i);
        viewContainer->setMinimumWidth(viewContainer->minimumSizeHint().width());
    }
    m_expandingContainer = nullptr;
}

void Lion ExplorerTabPage::slotAnimationValueChanged(const QVariant &value)
{
    Q_ASSERT(m_expandingContainer);
    const int indexOfExpandingContainer = m_splitter->indexOf(m_expandingContainer);
    int indexOfNonExpandingContainer = -1;
    if (m_expandingContainer == m_primaryViewContainer) {
        indexOfNonExpandingContainer = m_splitter->indexOf(m_secondaryViewContainer);
    } else {
        indexOfNonExpandingContainer = m_splitter->indexOf(m_primaryViewContainer);
    }
    std::vector<QWidget *> widgetsToRemove;
    const QList<int> oldSplitterSizes = m_splitter->sizes();
    QList<int> newSplitterSizes{oldSplitterSizes};
    int expansionWidthNeeded = value.toInt() - oldSplitterSizes.at(indexOfExpandingContainer);

    // Reduce the size of the other widgets to make space for the expandingContainer.
    for (int i = m_splitter->count() - 1; i >= 0; --i) {
        if (m_splitter->widget(i) == m_primaryViewContainer || m_splitter->widget(i) == m_secondaryViewContainer) {
            continue;
        }
        newSplitterSizes[i] = oldSplitterSizes.at(i) - expansionWidthNeeded;
        expansionWidthNeeded = 0;
        if (indexOfNonExpandingContainer != -1) {
            // Make sure every zombie container is at least slightly reduced in size
            // so it doesn't seem like they are here to stay.
            newSplitterSizes[i]--;
            newSplitterSizes[indexOfNonExpandingContainer]++;
        }
        if (newSplitterSizes.at(i) <= 0) {
            expansionWidthNeeded -= newSplitterSizes.at(i);
            newSplitterSizes[i] = 0;
            widgetsToRemove.emplace_back(m_splitter->widget(i));
        }
    }
    if (expansionWidthNeeded > 1 && indexOfNonExpandingContainer != -1) {
        Q_ASSERT(m_splitViewEnabled);
        newSplitterSizes[indexOfNonExpandingContainer] -= expansionWidthNeeded;
    }
    newSplitterSizes[indexOfExpandingContainer] = value.toInt();
    m_splitter->setSizes(newSplitterSizes);
    while (!widgetsToRemove.empty()) {
        widgetsToRemove.back()->close();
        widgetsToRemove.back()->deleteLater();
        widgetsToRemove.pop_back();
    }
}

void Lion ExplorerTabPage::slotViewActivated()
{
    const Lion ExplorerView *oldActiveView = activeViewContainer()->view();

    // Set the view, which was active before, to inactive
    // and update the active view type, if tab is active
    if (m_active) {
        if (m_splitViewEnabled) {
            activeViewContainer()->setActive(false);
            m_primaryViewActive = !m_primaryViewActive;
        } else {
            m_primaryViewActive = true;
            if (m_secondaryViewContainer) {
                m_secondaryViewContainer->setActive(false);
            }
        }
    }

    const Lion ExplorerView *newActiveView = activeViewContainer()->view();

    if (newActiveView == oldActiveView) {
        return;
    }

    disconnect(oldActiveView, &Lion ExplorerView::urlChanged, this, &Lion ExplorerTabPage::activeViewUrlChanged);
    connect(newActiveView, &Lion ExplorerView::urlChanged, this, &Lion ExplorerTabPage::activeViewUrlChanged);
    Q_EMIT activeViewChanged(activeViewContainer());
    Q_EMIT activeViewUrlChanged(activeViewContainer()->url());
}

void Lion ExplorerTabPage::slotViewUrlRedirection(const QUrl &oldUrl, const QUrl &newUrl)
{
    Q_UNUSED(oldUrl)
    Q_EMIT activeViewUrlChanged(newUrl);
}

void Lion ExplorerTabPage::switchActiveView()
{
    if (!m_splitViewEnabled) {
        return;
    }
    if (m_primaryViewActive) {
        m_secondaryViewContainer->setActive(true);
    } else {
        m_primaryViewContainer->setActive(true);
    }
}

void Lion ExplorerTabPage::connectViewActivatedSignals()
{
    connect(m_primaryViewContainer->view(), &Lion ExplorerView::activated, this, &Lion ExplorerTabPage::slotViewActivated);
    if (m_secondaryViewContainer) {
        connect(m_secondaryViewContainer->view(), &Lion ExplorerView::activated, this, &Lion ExplorerTabPage::slotViewActivated);
    }
}

void Lion ExplorerTabPage::disconnectViewActivatedSignals()
{
    disconnect(m_primaryViewContainer->view(), &Lion ExplorerView::activated, this, &Lion ExplorerTabPage::slotViewActivated);
    if (m_secondaryViewContainer) {
        disconnect(m_secondaryViewContainer->view(), &Lion ExplorerView::activated, this, &Lion ExplorerTabPage::slotViewActivated);
    }
}

Lion ExplorerViewContainer *Lion ExplorerTabPage::createViewContainer(const QUrl &url) const
{
    Lion ExplorerViewContainer *container = new Lion ExplorerViewContainer(url, m_splitter);
    container->setActive(false);

    const Lion ExplorerView *view = container->view();
    connect(view, &Lion ExplorerView::activated, this, &Lion ExplorerTabPage::slotViewActivated);

    connect(view, &Lion ExplorerView::toggleActiveViewRequested, this, &Lion ExplorerTabPage::switchActiveView);

    return container;
}

void Lion ExplorerTabPage::startExpandViewAnimation(Lion ExplorerViewContainer *expandingContainer)
{
    Q_ASSERT(expandingContainer);
    Q_ASSERT(expandingContainer == m_primaryViewContainer || expandingContainer == m_secondaryViewContainer);
    m_expandingContainer = expandingContainer;

    m_expandViewAnimation = new QVariantAnimation(m_splitter);
    m_expandViewAnimation->setDuration(2 * style()->styleHint(QStyle::SH_Widget_Animation_Duration, nullptr, this) * GlobalConfig::animationDurationFactor());
    for (int i = 0; i < m_splitter->count(); ++i) {
        m_splitter->widget(i)->setMinimumWidth(1);
    }
    connect(m_expandViewAnimation, &QAbstractAnimation::finished, this, &Lion ExplorerTabPage::slotAnimationFinished);
    connect(m_expandViewAnimation, &QVariantAnimation::valueChanged, this, &Lion ExplorerTabPage::slotAnimationValueChanged);

    m_expandViewAnimation->setStartValue(expandingContainer->width());
    if (m_splitViewEnabled) { // A new viewContainer is being opened.
        m_expandViewAnimation->setEndValue(m_splitterLastPosition ? (m_splitter->width() - m_splitterLastPosition) : (m_splitter->width() / 2));
        m_expandViewAnimation->setEasingCurve(QEasingCurve::OutCubic);
    } else { // A viewContainer is being closed.
        m_expandViewAnimation->setEndValue(m_splitter->width());
        m_expandViewAnimation->setEasingCurve(QEasingCurve::InCubic);
    }
    m_expandViewAnimation->start(QAbstractAnimation::DeleteWhenStopped);
}

Lion ExplorerTabPageSplitterHandle::Lion ExplorerTabPageSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
    : QSplitterHandle(orientation, parent)
    , m_mouseReleaseWasReceived(false)
{
}

bool Lion ExplorerTabPageSplitterHandle::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseButtonPress:
        m_mouseReleaseWasReceived = false;
        break;
    case QEvent::MouseButtonRelease:
        if (m_mouseReleaseWasReceived) {
            resetSplitterSizes(splitter());
        }
        m_mouseReleaseWasReceived = !m_mouseReleaseWasReceived;
        break;
    case QEvent::MouseButtonDblClick:
        m_mouseReleaseWasReceived = false;
        resetSplitterSizes(splitter());
        break;
    default:
        break;
    }

    return QSplitterHandle::event(event);
}

Lion ExplorerTabPageSplitter::Lion ExplorerTabPageSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
{
}

QSplitterHandle *Lion ExplorerTabPageSplitter::createHandle()
{
    return new Lion ExplorerTabPageSplitterHandle(orientation(), this);
}

#include "moc_lionexplorertabpage.cpp"
