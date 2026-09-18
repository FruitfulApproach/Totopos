/********************************************************************************
** Form generated from reading UI file 'CategoryDialog.ui'
**
** Created by: Qt User Interface Compiler version 6.11.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CATEGORYDIALOG_H
#define UI_CATEGORYDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAbstractButton>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_CategoryDialogClass
{
public:
    QVBoxLayout *verticalLayout;
    QStackedWidget *stack;
    QWidget *pageName;
    QFormLayout *formLayout;
    QLabel *nameLabel;
    QLineEdit *nameEdit;
    QLabel *nameHint;
    QWidget *pageProps;
    QVBoxLayout *propsLayout;
    QLabel *propsLabel;
    QCheckBox *hasProducts;
    QCheckBox *hasCoproducts;
    QCheckBox *hasEqualizers;
    QCheckBox *hasCoequalizers;
    QCheckBox *hasZeroObject;
    QCheckBox *hasKernels;
    QCheckBox *isAdditive;
    QCheckBox *isAbelian;
    QCheckBox *isConcrete;
    QCheckBox *isLocallySmall;
    QSpacerItem *propsSpacer;
    QHBoxLayout *buttonRow;
    QPushButton *backButton;
    QPushButton *nextButton;
    QSpacerItem *buttonSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CategoryDialogClass)
    {
        if (CategoryDialogClass->objectName().isEmpty())
            CategoryDialogClass->setObjectName("CategoryDialogClass");
        CategoryDialogClass->resize(440, 360);
        verticalLayout = new QVBoxLayout(CategoryDialogClass);
        verticalLayout->setObjectName("verticalLayout");
        stack = new QStackedWidget(CategoryDialogClass);
        stack->setObjectName("stack");
        pageName = new QWidget();
        pageName->setObjectName("pageName");
        formLayout = new QFormLayout(pageName);
        formLayout->setObjectName("formLayout");
        nameLabel = new QLabel(pageName);
        nameLabel->setObjectName("nameLabel");

        formLayout->setWidget(0, QFormLayout::ItemRole::LabelRole, nameLabel);

        nameEdit = new QLineEdit(pageName);
        nameEdit->setObjectName("nameEdit");

        formLayout->setWidget(0, QFormLayout::ItemRole::FieldRole, nameEdit);

        nameHint = new QLabel(pageName);
        nameHint->setObjectName("nameHint");
        nameHint->setWordWrap(true);

        formLayout->setWidget(1, QFormLayout::ItemRole::FieldRole, nameHint);

        stack->addWidget(pageName);
        pageProps = new QWidget();
        pageProps->setObjectName("pageProps");
        propsLayout = new QVBoxLayout(pageProps);
        propsLayout->setObjectName("propsLayout");
        propsLabel = new QLabel(pageProps);
        propsLabel->setObjectName("propsLabel");

        propsLayout->addWidget(propsLabel);

        hasProducts = new QCheckBox(pageProps);
        hasProducts->setObjectName("hasProducts");

        propsLayout->addWidget(hasProducts);

        hasCoproducts = new QCheckBox(pageProps);
        hasCoproducts->setObjectName("hasCoproducts");

        propsLayout->addWidget(hasCoproducts);

        hasEqualizers = new QCheckBox(pageProps);
        hasEqualizers->setObjectName("hasEqualizers");

        propsLayout->addWidget(hasEqualizers);

        hasCoequalizers = new QCheckBox(pageProps);
        hasCoequalizers->setObjectName("hasCoequalizers");

        propsLayout->addWidget(hasCoequalizers);

        hasZeroObject = new QCheckBox(pageProps);
        hasZeroObject->setObjectName("hasZeroObject");

        propsLayout->addWidget(hasZeroObject);

        hasKernels = new QCheckBox(pageProps);
        hasKernels->setObjectName("hasKernels");

        propsLayout->addWidget(hasKernels);

        isAdditive = new QCheckBox(pageProps);
        isAdditive->setObjectName("isAdditive");

        propsLayout->addWidget(isAdditive);

        isAbelian = new QCheckBox(pageProps);
        isAbelian->setObjectName("isAbelian");

        propsLayout->addWidget(isAbelian);

        isConcrete = new QCheckBox(pageProps);
        isConcrete->setObjectName("isConcrete");

        propsLayout->addWidget(isConcrete);

        isLocallySmall = new QCheckBox(pageProps);
        isLocallySmall->setObjectName("isLocallySmall");

        propsLayout->addWidget(isLocallySmall);

        propsSpacer = new QSpacerItem(20, 10, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        propsLayout->addItem(propsSpacer);

        stack->addWidget(pageProps);

        verticalLayout->addWidget(stack);

        buttonRow = new QHBoxLayout();
        buttonRow->setObjectName("buttonRow");
        backButton = new QPushButton(CategoryDialogClass);
        backButton->setObjectName("backButton");

        buttonRow->addWidget(backButton);

        nextButton = new QPushButton(CategoryDialogClass);
        nextButton->setObjectName("nextButton");

        buttonRow->addWidget(nextButton);

        buttonSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        buttonRow->addItem(buttonSpacer);

        buttonBox = new QDialogButtonBox(CategoryDialogClass);
        buttonBox->setObjectName("buttonBox");
        buttonBox->setStandardButtons(QDialogButtonBox::StandardButton::Cancel|QDialogButtonBox::StandardButton::Ok);

        buttonRow->addWidget(buttonBox);


        verticalLayout->addLayout(buttonRow);


        retranslateUi(CategoryDialogClass);
        QObject::connect(buttonBox, &QDialogButtonBox::accepted, CategoryDialogClass, qOverload<>(&QDialog::accept));
        QObject::connect(buttonBox, &QDialogButtonBox::rejected, CategoryDialogClass, qOverload<>(&QDialog::reject));

        stack->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(CategoryDialogClass);
    } // setupUi

    void retranslateUi(QDialog *CategoryDialogClass)
    {
        CategoryDialogClass->setWindowTitle(QCoreApplication::translate("CategoryDialogClass", "New category", nullptr));
        nameLabel->setText(QCoreApplication::translate("CategoryDialogClass", "Name", nullptr));
        nameEdit->setPlaceholderText(QCoreApplication::translate("CategoryDialogClass", "e.g. R-Mod, Sh(X), Vect_k", nullptr));
        nameHint->setText(QCoreApplication::translate("CategoryDialogClass", "The category this sketch lives in. Next: the structure it has.", nullptr));
        propsLabel->setText(QCoreApplication::translate("CategoryDialogClass", "Structure this category has:", nullptr));
        hasProducts->setText(QCoreApplication::translate("CategoryDialogClass", "Has products", nullptr));
        hasCoproducts->setText(QCoreApplication::translate("CategoryDialogClass", "Has coproducts", nullptr));
        hasEqualizers->setText(QCoreApplication::translate("CategoryDialogClass", "Has equalizers", nullptr));
        hasCoequalizers->setText(QCoreApplication::translate("CategoryDialogClass", "Has coequalizers", nullptr));
        hasZeroObject->setText(QCoreApplication::translate("CategoryDialogClass", "Has a zero object", nullptr));
        hasKernels->setText(QCoreApplication::translate("CategoryDialogClass", "Has kernels and cokernels", nullptr));
        isAdditive->setText(QCoreApplication::translate("CategoryDialogClass", "Additive (biproducts, hom-groups)", nullptr));
        isAbelian->setText(QCoreApplication::translate("CategoryDialogClass", "Abelian", nullptr));
        isConcrete->setText(QCoreApplication::translate("CategoryDialogClass", "Concrete (a faithful functor to Set)", nullptr));
        isLocallySmall->setText(QCoreApplication::translate("CategoryDialogClass", "Locally small", nullptr));
        backButton->setText(QCoreApplication::translate("CategoryDialogClass", "< Back", nullptr));
        nextButton->setText(QCoreApplication::translate("CategoryDialogClass", "Next >", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CategoryDialogClass: public Ui_CategoryDialogClass {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CATEGORYDIALOG_H
