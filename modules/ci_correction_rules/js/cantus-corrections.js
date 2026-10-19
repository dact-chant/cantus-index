(function ($, Backdrop) {
  'use strict';

  Backdrop.behaviors.ciCorrectionRulesReview = {
    attach: function (context) {
      $('.cantus-corrections-form', context).once('ci-correction-rules-review').each(function () {
        var $form = $(this);
        var $checkboxes = $form.find('.cantus-corrections-checkbox');
        var $chants = $form.find('.cantus-corrections-chant');
        var $toggle = $form.find('.cantus-corrections-select-all');

        function updateToggleText() {
          var allChecked = $checkboxes.length > 0 &&
            $checkboxes.filter(':checked').length === $checkboxes.length;

          $toggle.text(allChecked ? Backdrop.t('Deselect all') : Backdrop.t('Select all'));
        }

        $toggle.on('click', function (event) {
          event.preventDefault();

          var selectAll = $checkboxes.filter(':checked').length !== $checkboxes.length;
          $checkboxes.prop('checked', selectAll);
          updateToggleText();
        });

        $checkboxes.on('change', updateToggleText);

        $chants.on('click', function (event) {
          if ($(event.target).closest('a, input, label').length > 0) {
            return;
          }

          var $checkbox = $(this).find('.cantus-corrections-checkbox');
          $checkbox.prop('checked', !$checkbox.prop('checked')).trigger('change');
        });

        updateToggleText();
      });
    }
  };
})(jQuery, Backdrop);
